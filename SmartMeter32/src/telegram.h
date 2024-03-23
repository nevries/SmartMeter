#include <string>
#include <atomic>
#include <ArduinoHA.h>

class Telegram {
public:
  Telegram(const char* unique_id, 
      const char *name,
      HABaseDeviceType::NumberPrecision precision, 
      const char *unitOfMeasuremant, 
      const char *deviceClass, 
      const char *stateClass, 
      unsigned long updateInterval);
  void setValue(const char* value);
  bool publish();

private:
    HASensorNumber haSensorNumber;
    HABaseDeviceType::NumberPrecision precision;
    std::atomic<float> newValue;
    unsigned long lastUpdate;
    unsigned long updateInterval;
};