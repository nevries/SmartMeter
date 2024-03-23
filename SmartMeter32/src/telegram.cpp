static const char* TAG = "telegram";
#include "telegram.h"

Telegram::Telegram(const char* unique_id, 
      const char *name,
      HABaseDeviceType::NumberPrecision precision, 
      const char *unitOfMeasuremant, 
      const char *deviceClass, 
      const char *stateClass, 
      unsigned long updateInterval)
    : haSensorNumber(unique_id, precision) {
  this->lastUpdate = ULONG_MAX;
  this->updateInterval = updateInterval;
  this->precision = precision;
  haSensorNumber.setName(name);
  haSensorNumber.setDeviceClass(deviceClass);
  haSensorNumber.setStateClass(stateClass);
  haSensorNumber.setUnitOfMeasurement(unitOfMeasuremant);
  ESP_LOGI(TAG, "Telegram object for %s created.", unique_id);
}

void Telegram::setValue(const char* value) {
  newValue = std::stof(value);
  ESP_LOGD(TAG, "Got new value %f for %s", newValue.load(), haSensorNumber.getName());
}

bool Telegram::publish() {
  unsigned long now = millis();
  unsigned long elapsed;
  if (now < lastUpdate) {
    elapsed = now + (ULONG_MAX - lastUpdate);
  } else {
    elapsed = now - lastUpdate;
  }
  if (elapsed >= updateInterval)
  {
    lastUpdate = now;
    ESP_LOGD(TAG, "Sending data %f for sensor %s.", newValue.load(), haSensorNumber.getName());
    haSensorNumber.setValue(newValue);
  }
  return false;
}