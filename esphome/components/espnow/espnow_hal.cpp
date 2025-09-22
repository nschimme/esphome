#include "espnow_hal.h"

namespace esphome {
namespace espnow {

#ifdef USE_ESP32
ESPNowHAL *get_esp_now_hal();
#elif defined(USE_ESP8266)
ESPNowHAL *get_esp_now_hal();
#else
#error "Unsupported platform"
#endif

}  // namespace espnow
}  // namespace esphome
