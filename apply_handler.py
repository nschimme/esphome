import re
import os

path = 'esphome/components/sprinkler/sprinkler.cpp'
with open(path, 'r') as f:
    content = f.read()

# Define the new Handler methods implementation
handler_impl = '''
void RollingCycleSoakHandler::start_full_cycle(uint32_t max_runtime, float user_multiplier) {
  float adjusted_max_runtime = max_runtime * user_multiplier;
  if (this->max_cycle_duration_ > 0 && adjusted_max_runtime > this->max_cycle_duration_) {
    this->total_passes_ = std::ceil(adjusted_max_runtime / (float) this->max_cycle_duration_);
  } else {
    this->total_passes_ = 1;
  }
  this->internal_multiplier_ = 1.0f / (float) this->total_passes_;
  this->current_pass_ = 0;
  this->anchor_valid_ = false;
}

void RollingCycleSoakHandler::reset() {
  this->internal_multiplier_ = 1.0f;
  this->total_passes_ = 1;
  this->current_pass_ = 0;
  this->anchor_valid_ = false;
}

void RollingCycleSoakHandler::record_valve_finished(SprinklerValveOperator *vo) {
  if (this->anchor_valid_ || vo == nullptr) {
    return;
  }
  uint32_t run_duration = vo->run_duration();
  uint32_t time_remaining = vo->time_remaining();
  if (run_duration - time_remaining >= 2) {
    this->anchor_millis_ = millis();
    this->anchor_valid_ = true;
  }
}

uint32_t RollingCycleSoakHandler::calculate_soak_delay_ms() {
  if (!this->anchor_valid_ || this->soak_duration_ == 0) {
    return 0;
  }
  uint32_t elapsed = millis() - this->anchor_millis_;
  uint32_t soak_ms = this->soak_duration_ * 1000;
  if (elapsed < soak_ms) {
    return soak_ms - elapsed;
  }
  return 0;
}

void RollingCycleSoakHandler::advance_pass() {
  this->current_pass_++;
  this->anchor_valid_ = false;
}

uint32_t RollingCycleSoakHandler::estimate_soak_time_(Sprinkler *controller) {
  uint32_t total_enabled_runtime = controller->total_cycle_time_enabled_valves();
  uint32_t first_valve_runtime = 0;
  auto first_valve = controller->next_valve_number_(nullopt, false, false);
  if (first_valve.has_value()) {
    first_valve_runtime = controller->valve_run_duration_adjusted(*first_valve);
  }

  uint32_t other_valves_runtime =
      total_enabled_runtime > first_valve_runtime ? total_enabled_runtime - first_valve_runtime : 0;

  if (this->soak_duration_ > other_valves_runtime) {
    return this->soak_duration_ - other_valves_runtime;
  }
  return 0;
}
'''

# Insert handler implementation after SprinklerControllerSwitch::dump_config
content = re.sub(r'(void SprinklerControllerSwitch::dump_config\(\) \{ LOG_SWITCH\("", "Sprinkler Switch", this\); \})',
                r'\1' + handler_impl, content)

# Update Sprinkler setters
content = re.sub(r'void Sprinkler::set_max_cycle_duration\(uint32_t max_cycle_duration\) \{ this->max_cycle_duration_ = max_cycle_duration; \}',
                r'void Sprinkler::set_max_cycle_duration(uint32_t max_cycle_duration) {\n  this->cycle_soak_handler_.set_max_cycle_duration(max_cycle_duration);\n}', content)
content = re.sub(r'void Sprinkler::set_soak_duration\(uint32_t soak_duration\) \{ this->soak_duration_ = soak_duration; \}',
                r'void Sprinkler::set_soak_duration(uint32_t soak_duration) {\n  this->cycle_soak_handler_.set_soak_duration(soak_duration);\n}', content)

# Update valve_run_duration_adjusted
content = re.sub(r'run_duration = static_cast<uint32_t>\(roundf\(run_duration \* this->internal_fractional_multiplier_\)\);',
                r'run_duration = static_cast<uint32_t>(roundf(run_duration * this->cycle_soak_handler_.get_multiplier()));', content)

# Update start_from_queue and start_single_valve
content = re.sub(r'this->reset_cycle_soak_state_\(\);', r'this->cycle_soak_handler_.reset();', content)

# Update shutdown
content = re.sub(r'this->reset_cycle_soak_state_\(\);', r'this->cycle_soak_handler_.reset();', content)

# Update time_remaining_current_operation
new_telemetry = '''optional<uint32_t> Sprinkler::time_remaining_current_operation() {
  if (!this->time_remaining_active_valve().has_value() && this->state_ == IDLE) {
    return nullopt;
  }

  uint32_t total_time_remaining = 0;
  if (this->state_ == SOAKING) {
    uint32_t elapsed = millis() - this->timer_[sprinkler::TIMER_SM].start_time;
    uint32_t duration = this->timer_[sprinkler::TIMER_SM].time;
    if (duration > elapsed) {
      total_time_remaining = (duration - elapsed) / 1000;
    }
  } else {
    total_time_remaining = this->time_remaining_active_valve().value_or(0);
  }

  if (this->auto_advance()) {
    total_time_remaining += this->total_cycle_time_enabled_incomplete_valves();

    uint32_t total_enabled_runtime = this->total_cycle_time_enabled_valves();
    uint32_t soak_per_pass = this->cycle_soak_handler_.estimate_soak_time_(this);

    uint32_t total_passes = this->cycle_soak_handler_.total_passes();
    uint32_t current_pass = this->cycle_soak_handler_.current_pass();

    if (total_passes > 1) {
      uint32_t remaining_passes = (total_passes - 1) - current_pass;
      total_time_remaining += remaining_passes * (total_enabled_runtime + soak_per_pass);
    }

    if (this->repeat().value_or(0) > 0) {
      uint32_t remaining_repeats = this->repeat().value_or(0) - this->repeat_count().value_or(0);
      total_time_remaining += remaining_repeats * (total_enabled_runtime + soak_per_pass) * total_passes;
    }
  }

  if (this->queue_enabled()) {
    total_time_remaining += this->total_queue_time();
  }
  return total_time_remaining;
}'''
content = re.sub(r'optional<uint32_t> Sprinkler::time_remaining_current_operation\(\) \{.*?^\}', new_telemetry, content, flags=re.DOTALL|re.MULTILINE)

# Update prep_full_cycle_
new_prep = '''void Sprinkler::prep_full_cycle_() {
  this->set_auto_advance(true);

  if (!this->any_valve_is_enabled_()) {
    for (auto &valve : this->valve_) {
      if (valve.enable_switch != nullptr) {
        if (!valve.enable_switch->state) {
          valve.enable_switch->turn_on();
        }
      }
    }
  }

  uint32_t max_runtime = 0;
  for (size_t i = 0; i < this->number_of_valves(); i++) {
    if (this->valve_is_enabled_(i)) {
      max_runtime = std::max(max_runtime, this->valve_run_duration(i));
    }
  }
  this->cycle_soak_handler_.start_full_cycle(max_runtime, this->multiplier());

  this->reset_cycle_states_();
}'''
content = re.sub(r'void Sprinkler::prep_full_cycle_\(\) \{.*?^\}', new_prep, content, flags=re.DOTALL|re.MULTILINE)

# Update fsm_transition_from_valve_run_
new_fsm_valve_run = '''void Sprinkler::fsm_transition_from_valve_run_() {
  if (!this->active_req_.has_request()) {
    this->fsm_transition_to_shutdown_();
    return;
  }

  if (!this->timer_active_(sprinkler::TIMER_SM)) {
    if ((this->active_req_.request_is_from() == CYCLE) || (this->active_req_.request_is_from() == USER)) {
      this->mark_valve_cycle_complete_(this->active_req_.valve());
      this->cycle_soak_handler_.record_valve_finished(this->active_req_.valve_operator());
    }
  } else {
    ESP_LOGD(TAG, "Valve cycle interrupted - NOT flagging valve as complete and stopping current valve");
    for (auto &vo : this->valve_op_) {
      vo.stop();
    }
  }

  uint32_t old_pass = this->cycle_soak_handler_.current_pass();
  uint32_t old_repeat = this->repeat_count_;

  this->load_next_valve_run_request_(this->active_req_.valve());

  if (this->next_req_.has_request()) {
    bool new_pass = (this->next_req_.request_is_from() == CYCLE) &&
                    (this->cycle_soak_handler_.current_pass() != old_pass || this->repeat_count_ != old_repeat);

    if (new_pass) {
      uint32_t delay_needed_ms = this->cycle_soak_handler_.calculate_soak_delay_ms();
      if (delay_needed_ms > 0) {
        ESP_LOGD(TAG, "Soaking for %" PRIu32 " seconds", (delay_needed_ms + 999) / 1000);
        this->state_ = SOAKING;
        this->set_timer_duration_(sprinkler::TIMER_SM, (delay_needed_ms + 999) / 1000);
        this->start_timer_(sprinkler::TIMER_SM);
        return;
      }
    }

    auto *active_pump = this->valve_pump_switch(this->active_req_.valve());
    auto *next_pump = this->valve_pump_switch(this->next_req_.valve());
    bool same_pump = (active_pump != nullptr) && (next_pump != nullptr) && (active_pump == next_pump);

    this->active_req_.set_valve(this->next_req_.valve());
    this->active_req_.set_request_from(this->next_req_.request_is_from());
    this->active_req_.set_run_duration(this->next_req_.run_duration());
    this->next_req_.reset();

    if (this->valve_overlap_ || !this->switching_delay_.has_value()) {
      uint32_t timer_duration = this->active_req_.run_duration();
      if (timer_duration > this->switching_delay_.value_or(0)) {
        timer_duration -= this->switching_delay_.value_or(0);
      }
      this->set_timer_duration_(sprinkler::TIMER_SM, timer_duration);
      this->start_timer_(sprinkler::TIMER_SM);
      this->start_valve_(&this->active_req_);
    } else {
      this->set_timer_duration_(
          sprinkler::TIMER_SM,
          this->switching_delay_.value() * 2 +
              (this->pump_switch_off_during_valve_open_delay_ && same_pump ? this->stop_delay_ : 0));
      this->start_timer_(sprinkler::TIMER_SM);
      this->state_ = STARTING;
    }
  } else {
    this->fsm_transition_to_shutdown_();
  }
}'''
content = re.sub(r'void Sprinkler::fsm_transition_from_valve_run_\(\) \{.*?^\}', new_fsm_valve_run, content, flags=re.DOTALL|re.MULTILINE)

# Update load_next_valve_run_request_
content = re.sub(r'\} else if \(this->current_cycle_pass_ < this->total_cycle_passes_ - 1\) \{', r'} else if (this->cycle_soak_handler_.has_more_passes()) {', content)
content = re.sub(r'this->current_cycle_pass_\+\+;\s+this->rolling_soak_anchor_valid_ = false;', r'this->cycle_soak_handler_.advance_pass();', content)
content = re.sub(r'this->current_cycle_pass_ = 0;', r'// pass handled by start_full_cycle', content)

# Remove reset_cycle_soak_state_
content = re.sub(r'void Sprinkler::reset_cycle_soak_state_\(\) \{.*?^\}', '', content, flags=re.DOTALL|re.MULTILINE)

with open(path, 'w') as f:
    f.write(content)
