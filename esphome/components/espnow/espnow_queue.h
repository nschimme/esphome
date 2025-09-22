#pragma once

#ifdef USE_ESP32
#include "esphome/core/lock_free_queue.h"
#include "esphome/core/event_pool.h"
#else
#include <vector>

namespace esphome {
namespace espnow {

template<typename T, size_t SIZE>
class LockFreeQueue {
 public:
  bool push(T *item) {
    if (this->locked_)
      return false;
    this->locked_ = true;
    if (this->queue_.size() >= SIZE) {
      this->locked_ = false;
      return false;
    }
    this->queue_.push_back(item);
    this->locked_ = false;
    return true;
  }

  T *pop() {
    if (this->locked_)
      return nullptr;
    this->locked_ = true;
    if (this->queue_.empty()) {
      this->locked_ = false;
      return nullptr;
    }
    T *item = this->queue_.front();
    this->queue_.erase(this->queue_.begin());
    this->locked_ = false;
    return item;
  }

  void increment_dropped_count() { this->dropped_count_++; }
  uint16_t get_and_reset_dropped_count() {
    uint16_t dropped = this->dropped_count_;
    this->dropped_count_ = 0;
    return dropped;
  }

 private:
  std::vector<T *> queue_;
  bool locked_{false};
  uint16_t dropped_count_{0};
};

template<typename T, size_t SIZE>
class EventPool {
 public:
  T *allocate() { return new T(); }
  void release(T *event) { delete event; }
};

}  // namespace espnow
}  // namespace esphome
#endif
