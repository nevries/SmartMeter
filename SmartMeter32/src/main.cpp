static const char* TAG = "main";
#include <map>
#include <string>
#include <atomic>
#include <WebServer.h>
#include <ElegantOTA.h>
#include <Arduino.h>
#include <WiFi.h>
#include <logarex.h>
#include <telegram.h>

/* create this file and add your wlan and mqtt credentials
  const char* ssid = "MyWLANSID";
  const char* password = "MYPASSWORD";
  const char* mqtt_username   = "USERNAME";
  const char* mqtt_pass       = "PASSWORD";
  const uint16_t mqtt_port    = 1883;
*/
#include "../../../private.h"

#define HOSTNAME "Logarex-Bridge"


//======================================================================================================================
// Networking
//======================================================================================================================
#define BROKER_ADDR IPAddress(192,168,188,4) 
#define MQTT_PORT 1883
WiFiClient client;
WebServer server(80);

//======================================================================================================================
// HA Device Parameter
//======================================================================================================================
HADevice device("logarex");
HAMqtt mqtt(client, device);
HASensorNumber rs485_timeouts("logarex_rs485timeouts", HASensorNumber::PrecisionP0);
HASensorNumber rs485_checksumerrors("logarex_rs485checksumerrors", HASensorNumber::PrecisionP0);
HASensorNumber rs485_commerrors("logarex_rs485communicationerrors", HASensorNumber::PrecisionP0);

//======================================================================================================================
// Logarex stuff
//======================================================================================================================
void newTelegramCallback(const char *key, const char *value);
Logarex logarex(&Serial2, newTelegramCallback);
TaskHandle_t logarexTask;
std::map<std::string,Telegram *> telegrams;

void Wifi_disconnected(WiFiEvent_t event, WiFiEventInfo_t info){
  WiFi.disconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void Wifi_connected(WiFiEvent_t event, WiFiEventInfo_t info){
  ESP_LOGI(TAG, "Wifi %s : %s connected!", info.wifi_sta_connected.ssid, info.wifi_sta_connected.bssid);
}

void newTelegramCallback(const char *key, const char *value) {
  try {
    Telegram *t = telegrams.at(key);
    t->setValue(value);
  }
  catch (const std::out_of_range& oor) {
    ESP_LOGD(TAG, "Telegram key %s not handled.", key);
  }
}

void logarexPolling( void * parameter) {
  while(true){
      logarex.loop();
      vTaskDelay(1);    
  }
  vTaskDelete(NULL);
}

//======================================================================================================================
// Setups
//======================================================================================================================

void setup_wifi() {
  WiFi.setHostname(HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  //WiFi.setAutoReconnect(true);
  WiFi.onEvent(Wifi_connected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.onEvent(Wifi_disconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED); //SYSTEM_EVENT_STA_DISCONNECTED
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void setup_device(){
  device.setName("Logarex LK13BE803039");
  device.setSoftwareVersion("1.0.0");
  device.setManufacturer("Ragnar's Inc");
  device.setModel("ESP32");
  device.enableSharedAvailability();
  device.enableLastWill();

  rs485_timeouts.setName("Number of RS485 timeouts since start");
  rs485_timeouts.setStateClass("total_increasing");
  rs485_checksumerrors.setName("Number of RS485 checksum errors since start");
  rs485_checksumerrors.setStateClass("total_increasing");
  rs485_commerrors.setName("Number of RS485 protocol errores since start");
  rs485_commerrors.setStateClass("total_increasing");

  telegrams = {
    {"1-0:1.8.0*255", 
      new Telegram("logarex_energy1",
        "Energy 1", 
        HABaseDeviceType::PrecisionP3, 
        "kWh",
        "energy", 
        "total_increasing", 
        15 * 60'000)},
    {"1-0:2.8.0*255", 
      new Telegram("logarex_energy2",
        "Energy 2", 
        HABaseDeviceType::PrecisionP3, 
        "kWh",
        "energy", 
        "total_increasing", 
        15 * 60'000)},
    {"1-0:16.7.0*255", 
      new Telegram("logarex_power",
        "Power", 
        HABaseDeviceType::PrecisionP0, 
        "W",
        "power", 
        "measurement", 
        5'000)},
    {"1-0:32.7.0*255", 
      new Telegram("logarex_voltageL1",
        "Voltage L1", 
        HABaseDeviceType::PrecisionP1, 
        "V",
        "voltage", 
        "measurement", 
        5 * 60'000)},
    {"1-0:52.7.0*255", 
      new Telegram("logarex_voltageL2",
        "Voltage L2", 
        HABaseDeviceType::PrecisionP1, 
        "V",
        "voltage", 
        "measurement", 
        5 * 60'000)},
    {"1-0:72.7.0*255", 
      new Telegram("logarex_voltageL3",
        "Voltage L3", 
        HABaseDeviceType::PrecisionP1, 
        "V",
        "voltage", 
        "measurement", 
        5 * 60'000)},
    {"1-0:31.7.0*255", 
      new Telegram("logarex_currentL1",
        "Current L1", 
        HABaseDeviceType::PrecisionP2, 
        "A",
        "current", 
        "measurement", 
        5 * 60'000)},
    {"1-0:51.7.0*255", 
      new Telegram("logarex_currentL2",
        "Current L2", 
        HABaseDeviceType::PrecisionP2, 
        "A",
        "current", 
        "measurement", 
        5 * 60'000)},
    {"1-0:71.7.0*255", 
      new Telegram("logarex_currentL3",
        "Current L3", 
        HABaseDeviceType::PrecisionP2, 
        "A",
        "current", 
        "measurement", 
        5 * 60'000)},
    {"1-0:81.7.1*255", 
      new Telegram("logarex_phaseangleUL2UL1",
        "Phase angle UL2:UL1", 
        HABaseDeviceType::PrecisionP2, 
        "°",
        "None", 
        "measurement", 
        15 * 60'000)},
    {"1-0:81.7.2*255", 
      new Telegram("logarex_phaseangleUL3UL1",
        "Phase angle UL3:UL1", 
        HABaseDeviceType::PrecisionP2, 
        "°",
        "None", 
        "measurement", 
        15 * 60'000)},
    {"1-0:81.7.4*255", 
      new Telegram("logarex_phaseangleIL1UL1",
        "Phase angle IL1:UL1", 
        HABaseDeviceType::PrecisionP2, 
        "°",
        "None", 
        "measurement", 
        15 * 60'000)},
    {"1-0:81.7.15*255", 
      new Telegram("logarex_phaseangleIL2UL2",
        "Phase angle IL2:UL2", 
        HABaseDeviceType::PrecisionP2, 
        "°",
        "None", 
        "measurement", 
        15 * 60'000)},
    {"1-0:81.7.26*255", 
      new Telegram("logarex_phaseangleIL3UL3",
        "Phase angle IL3:UL3", 
        HABaseDeviceType::PrecisionP2, 
        "°",
        "None", 
        "measurement", 
        15 * 60'000)},
    {"1-0:14.7.0*255", 
      new Telegram("logarex_supplyfrequency",
        "Supply frequency", 
        HABaseDeviceType::PrecisionP2, 
        "Hz",
        "frequency", 
        "measurement", 
        15 * 60'000)},
  };
}

void setup_mqtt(){
  mqtt.begin(BROKER_ADDR, MQTT_PORT, MQTT_USER, MQTT_PASSWORD);
}

void setup() {
  // internal LED on while setting up things
  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(BUILTIN_LED, HIGH);

  ESP_LOGI(TAG, "Setup started");

  setup_device();

  ESP_LOGI(TAG, "HA device configured");

  // Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1);

  xTaskCreatePinnedToCore(
      logarexPolling, /* Function to implement the task */
      "LogarexTask", /* Name of the task */
      10000,  /* Stack size in words */
      NULL,  /* Task input parameter */
      //1,  /* Priority of the task */
      configMAX_PRIORITIES -1,
      &logarexTask,  /* Task handle. */
      1); /* Core where the task should run */

  ESP_LOGI(TAG, "RS485 initialized and loop started");

  setup_wifi();
  ESP_LOGI(TAG, "Wifi connected");

  setup_mqtt();
  ESP_LOGI(TAG, "MQTT started");

  server.on("/", []() {
    server.send(200, "text/plain", "Rags' Logarex Bridge Webserver for OTA.");
  });
  ElegantOTA.begin(&server);
  server.begin();
  Serial.println("HTTP server started");
  ESP_LOGI(TAG, "OTA server started");

  logarex.start();
  ESP_LOGI(TAG, "RS485 communication started");

  ESP_LOGI(TAG, "Setup done.");
  // setup done, internal LED off
  digitalWrite(BUILTIN_LED, LOW);
}


void loop() {
  server.handleClient();
  ElegantOTA.loop();
  mqtt.loop();
  
  rs485_checksumerrors.setValue((u_int32_t)logarex.countChecksumErrors());
  rs485_commerrors.setValue((u_int32_t)logarex.countProtocolErrors());
  rs485_timeouts.setValue((u_int32_t)logarex.countTimeouts());
    
  if (logarex.isValidDataReceived()) {
    for (const auto& kv : telegrams) {
      kv.second->publish();
    }
  }
}
