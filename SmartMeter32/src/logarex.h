#include <Stream.h>
#include <mutex>

enum StateMachine: uint8_t{
    IDLE,
    
    SEND_REQUEST,
    SEND_REQUEST_RETRY,
    
    RCV_ID,
    RCV_ID_RETRY,
    RCV_ID_MANUFACTURER1,
    RCV_ID_MANUFACTURER2,
    RCV_ID_MANUFACTURER3,
    RCV_ID_BAUDRATE,
    RCV_ID_ID,
    RCV_ID_END,
    
    SEND_ACK,
    SEND_ACK_RETRY,
    
    RCV_DATA,
    RCV_DATA_RETRY,
    RCV_DATA_TELEGRAM_KEY,
    RCV_DATA_TELEGRAM_VALUE,
    RCV_DATA_TELEGRAM_UNIT,
    RCV_DATA_TELEGRAM_END,
    RCV_DATA_END,
    RCV_DATA_CHECKSUM,

    CHECKSUM_ERROR,
    UNEXPECTED_INPUT,
    TIMEOUT,
    EMPTY_BUS,
};

class Logarex {
public:
  typedef std::function<void(const char *key, const char *value)> new_telegram_function_t;
  
  Logarex(Stream *port, new_telegram_function_t callback);

  bool isValidDataReceived();
  
  unsigned long countTimeouts();
  unsigned long countChecksumErrors();
  unsigned long countProtocolErrors();

  void start();
  void stop();

  void loop();

  void addToDelaySendQuery(int32_t positiveOrNegativeMS);
  void addToDelaySendACK(int32_t positiveOrNegativeMS);
  void addToTimeout(int32_t positiveOrNegativeMS);

private:
    Stream *port;
    bool validDataReceived = false;
    mutable std::mutex mtx;
    StateMachine state;
    unsigned char buffer[640];
    void unexpected(char last, char expected);
    new_telegram_function_t callback;
    unsigned long timeouts = 0;
    unsigned long checksumErrors = 0;
    unsigned long unexpectedInputs = 0;
    uint32_t delaySendQuery;
    uint32_t delaySendACK;
    uint32_t timeout;
};