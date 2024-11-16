static const char* TAG = "logarex";
#include <Arduino.h>
#include "logarex.h"

#define DELAY_SEND_MILLIS 75
#define TIMEOUT_MILLIS 4'000
#define DELAY_AFTER_FULL_READ 2'000

Logarex::Logarex(Stream *port, new_telegram_function_t callback) {
  this->port = port;
  state = IDLE;
  this->callback = callback;
  delaySendQuery = DELAY_SEND_MILLIS;
  delaySendACK = DELAY_SEND_MILLIS;
  timeout = TIMEOUT_MILLIS;
}

unsigned long Logarex::countTimeouts() {
  return timeouts;
}

unsigned long Logarex::countChecksumErrors() {
  return checksumErrors;
}
unsigned long Logarex::countProtocolErrors() {
  return unexpectedInputs;
}

bool Logarex::isValidDataReceived() {
  return validDataReceived;
}

void Logarex::start() {
  ESP_LOGI(TAG, "Starting Logarex");
  state = EMPTY_BUS;
}

void Logarex::stop() {
  ESP_LOGI(TAG, "Stopping Logarex");
  state = IDLE;
}

void Logarex::unexpected(char last, char expected) {
  ESP_LOGD(TAG, "Unexpected character %d found, expected %d", last, expected);
  ESP_LOGD(TAG, "Set state to UNEXPECTED_INPUT");
  state = UNEXPECTED_INPUT;
}

void Logarex::loop() {
  int lastByte = port->read();
  ESP_LOGV(TAG, "RAW LOG: %d (character '%c')", lastByte, lastByte);

  static unsigned long lastInputMillis = millis();
  // static unsigned long lastQuerySent = 0;
  // static unsigned long lastACKSent = 0;
  unsigned long currentMillis = millis();
  unsigned long elapsed = 0;
  if (lastByte != -1) {
    lastInputMillis = currentMillis;
  } else if (currentMillis >= lastInputMillis) {
    elapsed = currentMillis - lastInputMillis;
  } else {
    elapsed = currentMillis + (ULONG_MAX - lastInputMillis);
  }
  if ((state != IDLE) && (lastByte == -1) && (elapsed > timeout)) {
    ESP_LOGV(TAG, "Timeout triggered after %d millis in state %d", elapsed, state);
    switch(state)
    {
    case EMPTY_BUS:
      ESP_LOGV(TAG, "Set state to SEND_REQUEST");
      state = SEND_REQUEST;
      break;
    
    case RCV_ID:
      ESP_LOGV(TAG, "Set state to SEND_REQUEST_RETRY");
      state = SEND_REQUEST_RETRY;
      break;
    
    case RCV_DATA:
      ESP_LOGV(TAG, "Set state to SEND_ACK_RETRY");
      state = SEND_ACK_RETRY;
      break;
    
    default:
      state = TIMEOUT;
      ESP_LOGV(TAG, "Set state to TIMEOUT");
    }
    lastInputMillis = currentMillis;
  }

  static char manufacturer[4] = "";
  static char identification[16] = "";
  static int identification_pos = 0;
  static char telegram_key[20] = "";
  static int telegram_key_pos = 0;
  static char telegram_value[32] = "";
  static int telegram_value_pos = 0;
  static char telegram_unit[3] = "";
  static int telegram_unit_pos = 0;

  static byte bcc = 0;
  static bool calculate_bcc = false;
  if (lastByte != -1 && calculate_bcc) {
    bcc ^= (byte)lastByte;
  }

  switch (state)
  {
  case IDLE:
  case EMPTY_BUS:
    break;

  case SEND_REQUEST:
  case SEND_REQUEST_RETRY:
    if (lastByte == -1) {
      delay(delaySendQuery);
      port->print("/?!\r\n");
      if (state == SEND_REQUEST) {
        ESP_LOGV(TAG, "Set state to RCV_ID");
        state = RCV_ID;
      }
      if (state == SEND_REQUEST_RETRY) {
        ESP_LOGV(TAG, "Set state to RCV_ID_RETRY");
        state = RCV_ID_RETRY;
      }
      // lastQuerySent = currentMillis;
    } else {
      unexpected(lastByte, 0);
    }
    break;

  case RCV_ID:
  case RCV_ID_RETRY:
    if(lastByte == -1) break;
    if (lastByte == '/') {
      // Serial.print("QUERY: ");
      // Serial.println(currentMillis - lastQuerySent, DEC);
      ESP_LOGV(TAG, "Set state to RCV_ID_MANUFACTURER1");
      state = RCV_ID_MANUFACTURER1;
      manufacturer[0] = '\0';
    } else {
      unexpected(lastByte, '/');
    }
    break;

  case RCV_ID_MANUFACTURER1:
    if(lastByte == -1) break;
    manufacturer[0] = lastByte;
    ESP_LOGV(TAG, "Set state to RCV_ID_MANUFACTURER2");
    state = RCV_ID_MANUFACTURER2;
    break;

  case RCV_ID_MANUFACTURER2:
    if(lastByte == -1) break;
    manufacturer[1] = lastByte;
    ESP_LOGV(TAG, "Set state to RCV_ID_MANUFACTURER3");
    state = RCV_ID_MANUFACTURER3;
    break;

  case RCV_ID_MANUFACTURER3:
    if(lastByte == -1) break;
    manufacturer[2] = lastByte;
    manufacturer[3] = '\0';
    ESP_LOGV(TAG, "Set state to RCV_ID_BAUDRATE");
    state = RCV_ID_BAUDRATE;
    break;

  case RCV_ID_BAUDRATE:
    if(lastByte == -1) break;
    if(lastByte == '5') {
      ESP_LOGV(TAG, "Set state to RCV_ID_ID");
      state = RCV_ID_ID;
      identification[0] = '\0';
      identification_pos = 0;
    } else {
      unexpected(lastByte, '5');
    }
    break;

  case RCV_ID_ID:
    if(lastByte == -1) break;
    if(lastByte == '\r') {
      identification[identification_pos] = '\0';
      ESP_LOGV(TAG, "Set state to RCV_ID_END");
      state = RCV_ID_END;
    } else {
      identification[identification_pos++] = lastByte;
    }
    break;

  case RCV_ID_END:
    if(lastByte == -1) break;
    if(lastByte == '\n') {
      ESP_LOGV(TAG, "Set state to SEND_ACK");
      state = SEND_ACK;
      // TODO: Do something with the ID
    } else {
      unexpected(lastByte, '\n');
    }
    break;
  
  case SEND_ACK:
  case SEND_ACK_RETRY:
    if (lastByte == -1) {
      delay(delaySendACK);
      port->print("\006050\r\n"); 
      if (state == SEND_ACK) {
        ESP_LOGV(TAG, "Set state to RCV_DATA");
        state = RCV_DATA;
      }
      if (state == SEND_ACK_RETRY) {
        ESP_LOGV(TAG, "Set state to RCV_DATA_RETRY");
        state = RCV_DATA_RETRY;
      }
      // lastACKSent = currentMillis;
    } else {
      unexpected(lastByte, '\0');
    }
    break;
  
  case RCV_DATA:
  case RCV_DATA_RETRY:
    if(lastByte == -1) break;
    if(lastByte == '\002') {
      // Serial.print("ACK: ");
      // Serial.println(currentMillis - lastACKSent, DEC);
      bcc = 0;
      calculate_bcc = true;
      ESP_LOGV(TAG, "Set state to RCV_DATA_TELEGRAM_KEY");
      state = RCV_DATA_TELEGRAM_KEY;
      telegram_key[0] = '\0';
      telegram_key_pos = 0;
    } else {
      unexpected(lastByte, '\002');
    }
    break;
  
  case RCV_DATA_TELEGRAM_KEY:
    if(lastByte == -1) break;
    if(lastByte == '!') {
      ESP_LOGV(TAG, "Set state to RCV_DATA_END");
      state = RCV_DATA_END;
    } else if(lastByte == '(') {
      ESP_LOGV(TAG, "Set state to RCV_DATA_TELEGRAM_VALUE");
      state = RCV_DATA_TELEGRAM_VALUE;
      telegram_key[telegram_key_pos] = '\0';
      telegram_value[0] = '\0';
      telegram_value_pos = 0;
    } else {
      telegram_key[telegram_key_pos++] = lastByte;
    }
    break;

  case RCV_DATA_TELEGRAM_VALUE:
    if(lastByte == -1) break;
    if(lastByte == '*') {
      ESP_LOGV(TAG, "Set state to RCV_DATA_TELEGRAM_UNIT");
      state = RCV_DATA_TELEGRAM_UNIT;
      telegram_value[telegram_value_pos] = '\0';
      telegram_unit[0] = '\0';
      telegram_unit_pos = 0;
    } else if(lastByte == ')') {
      ESP_LOGV(TAG, "Set state to RCV_DATA_TELEGRAM_END");
      state = RCV_DATA_TELEGRAM_END;
      telegram_value[telegram_value_pos] = '\0';
    } else {
      telegram_value[telegram_value_pos++] = lastByte;
    }
    break;
  
  case RCV_DATA_TELEGRAM_UNIT:
    if(lastByte == -1) break;
    if(lastByte == ')') {
      ESP_LOGV(TAG, "Set state to RCV_DATA_TELEGRAM_END");
      state = RCV_DATA_TELEGRAM_END;
      telegram_unit[telegram_unit_pos] = '\0';
    } else {
      telegram_unit[telegram_unit_pos++] = lastByte;
    }
    break;
  
  case RCV_DATA_TELEGRAM_END:
    if(lastByte == -1) break;
    if(lastByte == '\r') break;
    if(lastByte == '\n') {
      ESP_LOGV(TAG, "Set state to RCV_DATA_TELEGRAM_KEY");
      state = RCV_DATA_TELEGRAM_KEY;
      ESP_LOGD(TAG, "Telegram received: k=%s, v=%s, u=%s", telegram_key, telegram_value, telegram_unit);
      
      ESP_LOGD(TAG, "Callback is: %d", callback);
      // Here the magic happens
      callback(telegram_key, telegram_value);

      telegram_key[0] = '\0';
      telegram_key_pos = 0;
    } else {
      unexpected(lastByte, '\n');
    }
    break;
  
  case RCV_DATA_END:
    if(lastByte == -1) break;
    if(lastByte == '\r') break;
    if(lastByte == '\n') break;
    if(lastByte == '\003') {
      calculate_bcc = false;
      ESP_LOGV(TAG, "Set state to RCV_DATA_CHECKSUM");
      state = RCV_DATA_CHECKSUM;
    } else {
      unexpected(lastByte, '\003');
    }
    break;
  
  case RCV_DATA_CHECKSUM:
    if(lastByte == -1) break;
    ESP_LOGV(TAG, "Received BCC byte %d and expected %d", lastByte, bcc);
    if(lastByte == bcc) {
      ESP_LOGV(TAG, "Set state to SEND_REQUEST");
      ESP_LOGI(TAG, "Dataset successfully reveiced");
      state = SEND_REQUEST;
      bcc = 0;
      validDataReceived = true;
    } else {
      ESP_LOGV(TAG, "Set state to CHECKSUM_ERROR");
      state = CHECKSUM_ERROR;
    }
    delay(DELAY_AFTER_FULL_READ);
    validDataReceived = false;
    break;
  
  case CHECKSUM_ERROR:
    ESP_LOGW(TAG, "Checksum error detected");
    checksumErrors++;
    ESP_LOGV(TAG, "Set state to EMPTY_BUS");
    state = EMPTY_BUS;
    break;
  
  case UNEXPECTED_INPUT:
    ESP_LOGW(TAG, "Unexpected input detected");
    unexpectedInputs++;
    ESP_LOGV(TAG, "Set state to EMPTY_BUS");
    state = EMPTY_BUS;
    break;

  case TIMEOUT:
    ESP_LOGW(TAG, "Timeout detected");
    timeouts++;
    ESP_LOGV(TAG, "Set state to EMPTY_BUS");
    state = EMPTY_BUS;
    break;
  
  default:
    break;
  }
}

void Logarex::addToDelaySendQuery(int32_t positiveOrNegativeMS)
{
  delaySendQuery += positiveOrNegativeMS;
  // Serial.print("Query delay: ");
  // Serial.println(delaySendQuery, DEC);
}

void Logarex::addToDelaySendACK(int32_t positiveOrNegativeMS)
{
  delaySendACK += positiveOrNegativeMS;
  // Serial.print("ACK delay: ");
  // Serial.println(delaySendACK, DEC);
}

void Logarex::addToTimeout(int32_t positiveOrNegativeMS)
{
  timeout += positiveOrNegativeMS;
  // Serial.print("Timeout: ");
  // Serial.println(timeout, DEC);
}
