#include "audio_decoder.h"

#ifdef USE_ESP32

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cstring>
#include <new>

namespace esphome::audio {

ESPHOME_LOG_TAG(TAG, "audio.decoder");

static const uint32_t READ_WRITE_TIMEOUT_MS = 20;  // Timeout for transferring audio data

// Max consecutive decode iterations that consume input but produce no output; e.g., skipping a large metadata block,
// before yielding and returning.
static const uint8_t MAX_NO_OUTPUT_ITERATIONS = 32;

static const uint32_t MAX_POTENTIALLY_FAILED_COUNT = 10;

#ifdef USE_AUDIO_AAC_SUPPORT
// Bytes needed to read an ADTS frame's length
static const size_t ADTS_HEADER_SIZE = 7;
// Worst-case PCM for one AAC frame (2048 samples, stereo, 16-bit); micro-aac reports 0 before the first frame
static const size_t MIN_AAC_OUTPUT_BYTES = 2048 * 2 * sizeof(int16_t);
#endif

AudioDecoder::AudioDecoder(size_t input_buffer_size, size_t output_buffer_size)
    : input_buffer_size_(input_buffer_size) {
  this->output_transfer_buffer_ = AudioSinkTransferBuffer::create(output_buffer_size);
}

esp_err_t AudioDecoder::add_source(std::weak_ptr<ring_buffer::RingBuffer> &input_ring_buffer) {
  // Zero-copy source reading directly from the ring buffer's internal storage. Raw file data is byte
  // aligned, so no frame alignment is required.
  auto source = RingBufferAudioSource::create(input_ring_buffer.lock(), this->input_buffer_size_);
  if (source == nullptr) {
    // create() only returns nullptr for invalid arguments (expired ring buffer or zero buffer size)
    return ESP_ERR_INVALID_ARG;
  }
  this->input_buffer_ = std::move(source);
  return ESP_OK;
}

esp_err_t AudioDecoder::add_source(const uint8_t *data_pointer, size_t length) {
  auto source = make_unique<ConstAudioSourceBuffer>();
  source->set_data(data_pointer, length);
  this->input_buffer_ = std::move(source);
  return ESP_OK;
}

esp_err_t AudioDecoder::add_sink(std::weak_ptr<ring_buffer::RingBuffer> &output_ring_buffer) {
  if (this->output_transfer_buffer_ != nullptr) {
    this->output_transfer_buffer_->set_sink(output_ring_buffer);
    return ESP_OK;
  }
  return ESP_ERR_NO_MEM;
}

#ifdef USE_SPEAKER
esp_err_t AudioDecoder::add_sink(speaker::Speaker *speaker) {
  if (this->output_transfer_buffer_ != nullptr) {
    this->output_transfer_buffer_->set_sink(speaker);
    return ESP_OK;
  }
  return ESP_ERR_NO_MEM;
}
#endif

esp_err_t AudioDecoder::add_sink(AudioSinkCallback *callback) {
  if (this->output_transfer_buffer_ != nullptr) {
    this->output_transfer_buffer_->set_sink(callback);
    return ESP_OK;
  }
  return ESP_ERR_NO_MEM;
}

esp_err_t AudioDecoder::start(AudioFileType audio_file_type) {
  if (this->output_transfer_buffer_ == nullptr) {
    return ESP_ERR_NO_MEM;
  }

  this->audio_file_type_ = audio_file_type;

  this->potentially_failed_count_ = 0;
  this->end_of_file_ = false;

  switch (this->audio_file_type_) {
#ifdef USE_AUDIO_AAC_SUPPORT
    case AudioFileType::ADTS:
      // Keep start() cheap: the pipeline calls it before attaching the source, and the reader can finish a small
      // file and release its ring buffer while a slow start() is still running. The AAC decoder itself is created
      // on the first decode call.
      this->adts_stage_.reset(new (std::nothrow) uint8_t[ADTS_STAGE_SIZE]);
      this->adts_stage_length_ = 0;
      this->aac_decoder_.reset();
      if (this->adts_stage_ == nullptr) {
        return ESP_ERR_NO_MEM;
      }
      this->free_buffer_required_ = MIN_AAC_OUTPUT_BYTES;
      break;
#endif
#ifdef USE_AUDIO_FLAC_SUPPORT
    case AudioFileType::FLAC:
      this->flac_decoder_ = make_unique<micro_flac::FLACDecoder>();
      this->free_buffer_required_ =
          this->output_transfer_buffer_->capacity();  // Adjusted and reallocated after reading the header
      break;
#endif
#ifdef USE_AUDIO_MP3_SUPPORT
    case AudioFileType::MP3:
      this->mp3_decoder_ = make_unique<micro_mp3::Mp3Decoder>();
      this->free_buffer_required_ =
          this->output_transfer_buffer_->capacity();  // Adjusted and reallocated after reading the header
      break;
#endif
#ifdef USE_AUDIO_OPUS_SUPPORT
    case AudioFileType::OPUS:
      this->opus_decoder_ = make_unique<micro_opus::OggOpusDecoder>();
      this->free_buffer_required_ =
          this->output_transfer_buffer_->capacity();  // Adjusted and reallocated after reading the header
      break;
#endif
#ifdef USE_AUDIO_WAV_SUPPORT
    case AudioFileType::WAV:
      this->wav_decoder_ = make_unique<micro_wav::WAVDecoder>();
      // 1 KiB suffices to always make progress while avoiding excessive CPU spinning for decoding
      this->free_buffer_required_ = 1024;
      if (this->output_transfer_buffer_->capacity() < this->free_buffer_required_) {
        this->output_transfer_buffer_->reallocate(this->free_buffer_required_);
      }
      break;
#endif
    case AudioFileType::NONE:
    default:
      return ESP_ERR_NOT_SUPPORTED;
      break;
  }

  return ESP_OK;
}

AudioDecoderState AudioDecoder::decode(bool stop_gracefully) {
  if (this->input_buffer_ == nullptr) {
    return AudioDecoderState::FAILED;
  }

  if (stop_gracefully) {
    if (this->output_transfer_buffer_->available() == 0) {
      if (this->end_of_file_) {
        // The file decoder indicates it reached the end of file
        return AudioDecoderState::FINISHED;
      }

      if (!this->input_buffer_->has_buffered_data()) {
        // If all the internal buffers are empty, the decoding is done
        return AudioDecoderState::FINISHED;
      }
    }
  }

  if (this->potentially_failed_count_ > MAX_POTENTIALLY_FAILED_COUNT) {
    if (stop_gracefully) {
      // No more new data is going to come in, so decoding is done
      return AudioDecoderState::FINISHED;
    }
    return AudioDecoderState::FAILED;
  }

  FileDecoderState state = FileDecoderState::MORE_TO_PROCESS;
  uint8_t no_output_iterations = 0;

  while (state == FileDecoderState::MORE_TO_PROCESS) {
    // Transfer decoded out
    if (!this->pause_output_) {
      // Never shift the data in the output transfer buffer to avoid unnecessary, slow data moves
      size_t bytes_written =
          this->output_transfer_buffer_->transfer_data_to_sink(pdMS_TO_TICKS(READ_WRITE_TIMEOUT_MS), false);

      if (this->audio_stream_info_.has_value()) {
        this->accumulated_frames_written_ += this->audio_stream_info_.value().bytes_to_frames(bytes_written);
        this->playback_ms_ +=
            this->audio_stream_info_.value().frames_to_milliseconds_with_remainder(&this->accumulated_frames_written_);
      }

      if ((bytes_written > 0) && (this->output_transfer_buffer_->available() == 0)) {
        // All decoded audio has been flushed to the sink; return so the caller can react to stop/pause before
        // decoding the next batch
        return AudioDecoderState::DECODING;
      }
    } else {
      // If paused, block to avoid wasting CPU resources
      delay(READ_WRITE_TIMEOUT_MS);
    }

    if (this->output_transfer_buffer_->available() > 0) {
      // Output transfer buffer indicates backpressure, return so caller can handle other events;
      // e.g., stop/pause, before trying again
      return AudioDecoderState::DECODING;
    }

    // Reaching here means no decoded output is pending (any would have returned above). Bounds long no-output
    // stretches; e.g., skipping a large metadata block, so a source that keeps the ring buffer full can't spin this
    // loop without yielding and trip the watchdog. The delay yields allowing other tasks to feed the watchdog and
    // the return keeps stop/pause responsive.
    if (++no_output_iterations >= MAX_NO_OUTPUT_ITERATIONS) {
      delay(1);
      return AudioDecoderState::DECODING;
    }

    // Expose the next chunk of file data. Every decoder buffers internally and consumes only what it
    // processed, so the source does not need to accumulate or stitch chunks across fill() calls.
    this->input_buffer_->fill(pdMS_TO_TICKS(READ_WRITE_TIMEOUT_MS));

    const size_t available_before_decode = this->input_buffer_->available();

    if (available_before_decode == 0) {
      // No data to decode, attempt to get more data next time
      state = FileDecoderState::IDLE;
    } else {
      switch (this->audio_file_type_) {
#ifdef USE_AUDIO_AAC_SUPPORT
        case AudioFileType::ADTS:
          state = this->decode_adts_();
          break;
#endif
#ifdef USE_AUDIO_FLAC_SUPPORT
        case AudioFileType::FLAC:
          state = this->decode_flac_();
          break;
#endif
#ifdef USE_AUDIO_MP3_SUPPORT
        case AudioFileType::MP3:
          state = this->decode_mp3_();
          break;
#endif
#ifdef USE_AUDIO_OPUS_SUPPORT
        case AudioFileType::OPUS:
          state = this->decode_opus_();
          break;
#endif
#ifdef USE_AUDIO_WAV_SUPPORT
        case AudioFileType::WAV:
          state = this->decode_wav_();
          break;
#endif
        case AudioFileType::NONE:
        default:
          state = FileDecoderState::IDLE;
          break;
      }
    }

    if (state == FileDecoderState::POTENTIALLY_FAILED) {
      ++this->potentially_failed_count_;
    } else if (state == FileDecoderState::END_OF_FILE) {
      this->end_of_file_ = true;
    } else if (state == FileDecoderState::FAILED) {
      return AudioDecoderState::FAILED;
    } else if (state == FileDecoderState::MORE_TO_PROCESS) {
      // Reset the failsafe only when the iteration made forward progress: input was consumed or output was
      // produced (output_transfer_buffer_ is drained empty above, so any available bytes are new). A
      // MORE_TO_PROCESS that neither consumes input nor produces output means the decoder is stalled; count it
      // toward the failsafe so a stuck stream eventually surfaces as FAILED instead of looping forever.
      if ((this->input_buffer_->available() < available_before_decode) ||
          (this->output_transfer_buffer_->available() > 0)) {
        this->potentially_failed_count_ = 0;
      } else {
        ++this->potentially_failed_count_;
      }
    }
  }
  return AudioDecoderState::DECODING;
}

#ifdef USE_AUDIO_AAC_SUPPORT
FileDecoderState AudioDecoder::decode_adts_() {
  if (this->aac_decoder_ == nullptr) {
    auto decoder = make_unique<micro_aac::AACDecoder>();
    micro_aac_dec_config_t config;
    micro_aac_dec_config_init(&config);
    config.stream_format = MICRO_AAC_DEC_STREAM_ADTS;
    config.prefer_psram = true;
    // The audio pipeline only handles mono and stereo
    config.downmix = MICRO_AAC_DEC_DOWNMIX_STEREO;
    if (decoder->init(config) != MICRO_AAC_DEC_OK) {
      ESP_LOGE(TAG, "AAC decoder failed to initialize");
      return FileDecoderState::FAILED;
    }
    // The decoder reports the worst-case PCM size of one frame; the output buffer must fit it
    this->free_buffer_required_ = std::max<size_t>(decoder->get_max_output_bytes(), MIN_AAC_OUTPUT_BYTES);
    if (this->output_transfer_buffer_->capacity() < this->free_buffer_required_ &&
        !this->output_transfer_buffer_->reallocate(this->free_buffer_required_)) {
      return FileDecoderState::FAILED;
    }
    this->aac_decoder_ = std::move(decoder);
  }

  // micro-aac finds the ADTS sync word itself and skips junk; it advances the input pointer past what it consumed.
  // The sample count it reports is total across channels.
  const bool staged = this->adts_stage_length_ > 0;
  size_t input_consumed = 0;
  uint8_t *input;
  int32_t available;

  if (staged) {
    // Complete the held partial frame with only as many bytes as it needs, so the stage empties after this frame and
    // later frames decode straight from the source again
    uint8_t *stage = this->adts_stage_.get();
    size_t wanted = ADTS_STAGE_SIZE - this->adts_stage_length_;
    if (this->adts_stage_length_ < ADTS_HEADER_SIZE) {
      wanted = ADTS_HEADER_SIZE - this->adts_stage_length_;
    } else if (stage[0] == 0xFF && (stage[1] & 0xF6) == 0xF0) {
      const size_t frame_length = ((stage[3] & 0x03) << 11) | (stage[4] << 3) | (stage[5] >> 5);
      if (frame_length >= ADTS_HEADER_SIZE && frame_length <= ADTS_STAGE_SIZE) {
        wanted = frame_length > this->adts_stage_length_ ? frame_length - this->adts_stage_length_ : 0;
      }
    }
    const size_t to_copy =
        std::min({this->input_buffer_->available(), ADTS_STAGE_SIZE - this->adts_stage_length_, wanted});
    std::memcpy(stage + this->adts_stage_length_, this->input_buffer_->data(), to_copy);
    this->input_buffer_->consume(to_copy);
    input_consumed = to_copy;
    this->adts_stage_length_ += to_copy;
    input = this->adts_stage_.get();
    available = static_cast<int32_t>(this->adts_stage_length_);
  } else {
    input = const_cast<uint8_t *>(this->input_buffer_->data());
    available = static_cast<int32_t>(this->input_buffer_->available());
  }

  int32_t bytes_left = available;
  uint32_t samples_decoded = 0;

  micro_aac_dec_status_t result = this->aac_decoder_->decode(
      &input, &bytes_left, reinterpret_cast<int16_t *>(this->output_transfer_buffer_->get_buffer_end()),
      static_cast<uint32_t>(this->output_transfer_buffer_->free()), &samples_decoded);

  const size_t decoder_consumed = static_cast<size_t>(available - bytes_left);
  if (staged) {
    // Keep only what the decoder left unread
    std::memmove(this->adts_stage_.get(), input, bytes_left);
    this->adts_stage_length_ = bytes_left;
  } else {
    if (result == MICRO_AAC_DEC_ERR_UNDERFLOW && bytes_left > 0 && static_cast<size_t>(bytes_left) < ADTS_STAGE_SIZE) {
      // The source ends in a partial frame: hold it until the next chunk arrives
      std::memcpy(this->adts_stage_.get(), input, bytes_left);
      this->adts_stage_length_ = bytes_left;
      input_consumed = static_cast<size_t>(available);
    } else {
      input_consumed = decoder_consumed;
    }
    this->input_buffer_->consume(input_consumed);
  }

  if (result == MICRO_AAC_DEC_ERR_UNDERFLOW && this->adts_stage_length_ >= ADTS_STAGE_SIZE) {
    // A full stage with no complete frame is not valid ADTS; drop it and resync
    this->adts_stage_length_ = 0;
    return FileDecoderState::POTENTIALLY_FAILED;
  }

  if (result == MICRO_AAC_DEC_OK) {
    micro_aac_dec_frame_info_t info;
    if (this->aac_decoder_->get_frame_info(&info) != MICRO_AAC_DEC_OK) {
      return FileDecoderState::FAILED;
    }
    // The rate can differ from the header's when SBR doubles it, so always take it from the decoded frame
    const audio::AudioStreamInfo stream_info(16, info.num_channels, info.sample_rate);
    if (!this->audio_stream_info_.has_value() || !(this->audio_stream_info_.value() == stream_info)) {
      this->audio_stream_info_ = stream_info;
    }
    if (samples_decoded > 0) {
      this->output_transfer_buffer_->increase_buffer_length(stream_info.samples_to_bytes(samples_decoded));
    }
  } else if (result == MICRO_AAC_DEC_ERR_UNDERFLOW) {
    // Partial frame; wait for more data. Input was consumed if it moved into the stage.
    return input_consumed > 0 ? FileDecoderState::MORE_TO_PROCESS : FileDecoderState::IDLE;
  } else if (result == MICRO_AAC_DEC_ERR_BUFFER_TOO_SMALL) {
    this->free_buffer_required_ = std::max<size_t>(this->aac_decoder_->get_max_output_bytes(), MIN_AAC_OUTPUT_BYTES);
    if (!this->output_transfer_buffer_->reallocate(this->free_buffer_required_)) {
      return FileDecoderState::FAILED;
    }
  } else if (result == MICRO_AAC_DEC_ERR_BAD_HEADER || result == MICRO_AAC_DEC_ERR_DECODE_FAILED) {
    // Lost sync or a corrupt frame; the decoder has skipped ahead, so retry on the next call
    ESP_LOGW(TAG, "AAC decoder skipped a corrupt frame");
    return FileDecoderState::POTENTIALLY_FAILED;
  } else {
    ESP_LOGE(TAG, "AAC decoder failed: %d", static_cast<int>(result));
    return FileDecoderState::FAILED;
  }

  return FileDecoderState::MORE_TO_PROCESS;
}
#endif

#ifdef USE_AUDIO_FLAC_SUPPORT
FileDecoderState AudioDecoder::decode_flac_() {
  size_t bytes_consumed, samples_decoded;

  micro_flac::FLACDecoderResult result = this->flac_decoder_->decode(
      this->input_buffer_->data(), this->input_buffer_->available(), this->output_transfer_buffer_->get_buffer_end(),
      this->output_transfer_buffer_->free(), bytes_consumed, samples_decoded);

  if (result == micro_flac::FLAC_DECODER_SUCCESS) {
    if (samples_decoded > 0 && this->audio_stream_info_.has_value()) {
      this->output_transfer_buffer_->increase_buffer_length(
          this->audio_stream_info_.value().samples_to_bytes(samples_decoded));
    }
    this->input_buffer_->consume(bytes_consumed);
  } else if (result == micro_flac::FLAC_DECODER_HEADER_READY) {
    // Header just parsed, stream info now available
    const auto &info = this->flac_decoder_->get_stream_info();
    this->audio_stream_info_ = audio::AudioStreamInfo(info.bits_per_sample(), info.num_channels(), info.sample_rate());

    // Reallocate the output transfer buffer to the required size
    this->free_buffer_required_ = this->flac_decoder_->get_output_buffer_size_samples() * info.bytes_per_sample();
    if (!this->output_transfer_buffer_->reallocate(this->free_buffer_required_)) {
      return FileDecoderState::FAILED;
    }
    this->input_buffer_->consume(bytes_consumed);
  } else if (result == micro_flac::FLAC_DECODER_END_OF_STREAM) {
    this->input_buffer_->consume(bytes_consumed);
    return FileDecoderState::END_OF_FILE;
  } else if (result == micro_flac::FLAC_DECODER_NEED_MORE_DATA) {
    this->input_buffer_->consume(bytes_consumed);
    return FileDecoderState::MORE_TO_PROCESS;
  } else if (result == micro_flac::FLAC_DECODER_ERROR_OUTPUT_TOO_SMALL) {
    // Reallocate to decode the frame on the next call
    const auto &info = this->flac_decoder_->get_stream_info();
    this->free_buffer_required_ = this->flac_decoder_->get_output_buffer_size_samples() * info.bytes_per_sample();
    if (!this->output_transfer_buffer_->reallocate(this->free_buffer_required_)) {
      return FileDecoderState::FAILED;
    }
  } else {
    ESP_LOGE(TAG, "FLAC decoder failed: %d", static_cast<int>(result));
    return FileDecoderState::POTENTIALLY_FAILED;
  }

  return FileDecoderState::MORE_TO_PROCESS;
}
#endif

#ifdef USE_AUDIO_MP3_SUPPORT
FileDecoderState AudioDecoder::decode_mp3_() {
  // microMP3's samples_decoded value is samples per channel; e.g., what ESPHome typically calls an audio frame.
  // microMP3 uses the term frame to refer to an MP3 frame: an encoded packet that contains multiple audio frames.
  size_t bytes_consumed = 0;
  size_t samples_decoded = 0;

  // microMP3 buffers internally: it consumes from our input buffer at its own pace, emits MP3_STREAM_INFO_READY once
  // the first frame header is parsed, and only then produces PCM. It handles sync-word search and ID3v2 tag skipping.
  micro_mp3::Mp3Result result = this->mp3_decoder_->decode(
      this->input_buffer_->data(), this->input_buffer_->available(), this->output_transfer_buffer_->get_buffer_end(),
      this->output_transfer_buffer_->free(), bytes_consumed, samples_decoded);

  this->input_buffer_->consume(bytes_consumed);

  if (result == micro_mp3::MP3_OK) {
    if (samples_decoded > 0 && this->audio_stream_info_.has_value()) {
      this->output_transfer_buffer_->increase_buffer_length(
          this->audio_stream_info_.value().frames_to_bytes(samples_decoded));
    }
  } else if (result == micro_mp3::MP3_STREAM_INFO_READY || result == micro_mp3::MP3_STREAM_INFO_CHANGED) {
    // Header parsed: capture stream info and resize the output buffer to fit one full frame.
    // microMP3 always outputs 16-bit PCM. MP3_STREAM_INFO_CHANGED is handled identically: despite its
    // negative value it is documented as recoverable, so it must not reach the catch-all below.
    this->audio_stream_info_ =
        audio::AudioStreamInfo(16, this->mp3_decoder_->get_channels(), this->mp3_decoder_->get_sample_rate());
    this->free_buffer_required_ =
        this->mp3_decoder_->get_samples_per_frame() * this->mp3_decoder_->get_channels() * sizeof(int16_t);
    if (!this->output_transfer_buffer_->reallocate(this->free_buffer_required_)) {
      return FileDecoderState::FAILED;
    }
  } else if (result == micro_mp3::MP3_NEED_MORE_DATA) {
    return FileDecoderState::MORE_TO_PROCESS;
  } else if (result == micro_mp3::MP3_OUTPUT_BUFFER_TOO_SMALL) {
    // Fallback to worst-case size
    this->free_buffer_required_ = this->mp3_decoder_->get_min_output_buffer_bytes();
    if (!this->output_transfer_buffer_->reallocate(this->free_buffer_required_)) {
      return FileDecoderState::FAILED;
    }
  } else if (result == micro_mp3::MP3_DECODE_ERROR) {
    // Corrupt frame skipped; recoverable, retry on next call
    ESP_LOGW(TAG, "MP3 decoder skipped a corrupt frame");
    return FileDecoderState::POTENTIALLY_FAILED;
  } else {
    // MP3_ALLOCATION_FAILED, MP3_INPUT_INVALID, or any future error -- not recoverable
    ESP_LOGE(TAG, "MP3 decoder failed: %d", static_cast<int>(result));
    return FileDecoderState::FAILED;
  }

  return FileDecoderState::MORE_TO_PROCESS;
}
#endif

#ifdef USE_AUDIO_OPUS_SUPPORT
FileDecoderState AudioDecoder::decode_opus_() {
  bool processed_header = this->opus_decoder_->is_initialized();

  size_t bytes_consumed, samples_decoded;

  micro_opus::OggOpusResult result = this->opus_decoder_->decode(
      this->input_buffer_->data(), this->input_buffer_->available(), this->output_transfer_buffer_->get_buffer_end(),
      this->output_transfer_buffer_->free(), bytes_consumed, samples_decoded);

  if (result == micro_opus::OGG_OPUS_OK) {
    if (!processed_header && this->opus_decoder_->is_initialized()) {
      // Header processed and stream info is available
      this->audio_stream_info_ =
          audio::AudioStreamInfo(this->opus_decoder_->get_bit_depth(), this->opus_decoder_->get_channels(),
                                 this->opus_decoder_->get_sample_rate());
    }
    if (samples_decoded > 0 && this->audio_stream_info_.has_value()) {
      // Some audio was processed
      this->output_transfer_buffer_->increase_buffer_length(
          this->audio_stream_info_.value().frames_to_bytes(samples_decoded));
    }
    this->input_buffer_->consume(bytes_consumed);
  } else if (result == micro_opus::OGG_OPUS_OUTPUT_BUFFER_TOO_SMALL) {
    // Reallocate to decode the packet on the next call
    this->free_buffer_required_ = this->opus_decoder_->get_required_output_buffer_size();
    if (!this->output_transfer_buffer_->reallocate(this->free_buffer_required_)) {
      // Couldn't reallocate output buffer
      return FileDecoderState::FAILED;
    }
  } else {
    ESP_LOGE(TAG, "Opus decoder failed: %" PRId8, result);
    return FileDecoderState::POTENTIALLY_FAILED;
  }
  return FileDecoderState::MORE_TO_PROCESS;
}
#endif

#ifdef USE_AUDIO_WAV_SUPPORT
FileDecoderState AudioDecoder::decode_wav_() {
  // microWAV's samples_decoded counts individual channel samples; e.g., for
  // 16-bit stereo, 4 input bytes results in 2 samples_decoded.
  size_t bytes_consumed = 0;
  size_t samples_decoded = 0;

  micro_wav::WAVDecoderResult result = this->wav_decoder_->decode(
      this->input_buffer_->data(), this->input_buffer_->available(), this->output_transfer_buffer_->get_buffer_end(),
      this->output_transfer_buffer_->free(), bytes_consumed, samples_decoded);

  this->input_buffer_->consume(bytes_consumed);

  if (result == micro_wav::WAV_DECODER_SUCCESS) {
    if (samples_decoded > 0 && this->audio_stream_info_.has_value()) {
      this->output_transfer_buffer_->increase_buffer_length(
          this->audio_stream_info_.value().samples_to_bytes(samples_decoded));
    }
  } else if (result == micro_wav::WAV_DECODER_HEADER_READY) {
    // After HEADER_READY, get_bits_per_sample() returns the output bit depth
    // (16 for A-law/mu-law, 32 for IEEE float, original value for PCM).
    this->audio_stream_info_ =
        audio::AudioStreamInfo(this->wav_decoder_->get_bits_per_sample(), this->wav_decoder_->get_channels(),
                               this->wav_decoder_->get_sample_rate());
  } else if (result == micro_wav::WAV_DECODER_NEED_MORE_DATA) {
    return FileDecoderState::MORE_TO_PROCESS;
  } else if (result == micro_wav::WAV_DECODER_END_OF_STREAM) {
    return FileDecoderState::END_OF_FILE;
  } else {
    ESP_LOGE(TAG, "WAV decoder failed: %d", static_cast<int>(result));
    return FileDecoderState::FAILED;
  }

  return FileDecoderState::MORE_TO_PROCESS;
}
#endif

}  // namespace esphome::audio

#endif
