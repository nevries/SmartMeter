Read your iec62056-21 protocol (DIN EN 62056-21) PULL mode smart meter via RS485 (**not** IR) into home assistant using this ESP32 software.

# Hardware

* Smart meter speaking iec62056-21 with RS485 port that is accessible to you (I tested only on [Logarex LK13BE803039](bedienungsanleitung-logarex-lk13be904619.pdf), but others should work fine with maybe small adjustments)
* ESP32 (I'm using [this one](https://www.az-delivery.de/products/esp32-developmentboard?variant=36542176914))
* RS485 to TTL converter module (I'm using [these](https://www.amazon.de/JZK-TTL-auf-RS485-Modul-UART-Niveau-Konvertierung-Durchflusskontrolle-RS485/dp/B09VGJCJKQ/))

# Circuit

![Circuit showing the smart meter Logarex LK13BE803039, an ESP32 dev board and an RS485 to TTL module](img/circuit.svg)

You might want to connect earth to the RS485 module if you assume a significant potential difference between your smart meter and the ESP32 power supply.

# Software

Just load the  [SmartMeter32](SmartMeter32) into PlaformIO to compile and upload the firmware to your ESP32 device. Please note that you need to adjust some minor things, at a minimum you need to create a private.h file containing your wifi credentials (see [main.cpp](SmartMeter32/src/main.cpp) for explanation) and in main.cpp adjust the connection parameters to your MQTT broker.

In addition, you might want to adjust the [list of sensors](https://github.com/nevries/SmartMeter/blob/5b92f47440a54a62801aa8dd5f6fea0bb2bd38e6/SmartMeter32/src/main.cpp#L101-L223 in platformio.ini) and/or their OBIS to match your device. If you don't know what your device provides, I recommend to start with an empty list of sensors and enable debugging (set `build_flags = -DCORE_DEBUG_LEVEL=4`), then connect to your smart meter and run the ESP32 connected to a serial monitor (for example in PlatformIO), it will print out all received OBIS including data on the serial output.

# Helpful Resources
* [HA Device and State classes](https://developers.home-assistant.io/docs/core/entity/sensor/)
* [IEC62056-21en spec](iec62056-21en.pdf)
* [Logarex LK13BE904xxx manual](bedienungsanleitung-logarex-lk13be904619.pdf)
* [Sample reading of my smart meter](example_read.txt)