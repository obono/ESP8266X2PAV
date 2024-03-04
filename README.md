# ESP8266X2PAV (ESP8266 Hexadeca-Squared Pixel Art Viewer)

## Description

A 16&times;16-dotted pixel art viewer with **ESP-WROOM-02** and **Unicorn HAT HD**.

## Hardware

:construction:

### Components

* [ESP-WROOM-02](https://store.arduino.cc/usa/arduino-nano) (or compatible product)
* [Unicorn HAT HD](https://shop.pimoroni.com/products/unicorn-hat-hd)

### Circuit diagram

:construction:

## Software

### Build and transfer

Clone the source code and open the project file "ESP8266SmartRemocon.ino" with Arduino IDE.

You must import ESP8266 board.

You must modify ["credential.h"](credential.h) according to your situation.

* `HOSTNAME` and `PORT` if you like.
    * The URL to get version information will be http://esp8266x2pav.local:8080/version if you don't modify.
* `STA_SSID` and `STA_PASSWORD` according to your Wi-Fi access point setting.
    * If you'd like to assign static IP address to the device, uncomment `#ifdef STATIC_ADDRESS` and edit values of `STATIC_ADDRESS_***`.

You can build the source code with following configuration.

Attribute        |Value
-----------------|------------------------------------
Board            |Generic ESP8266 Module
Builtin Led      |2
Upload Speed     |921600
CPU Frequency    |80 MHz
Crystal Frequency|26 MHz
Flash Size       |4MB (FS:3MB OTA:~512KB)
Flash Mode       |DOUT (compatible)
Flash Frequency  |40 MHz
Reset Method     |dtr (aka modemcu)
Debug port       |Disabled
Debug Level      |None
lwIP Variant     |v2 Lower Memory
VTables          |Flash
C++ Exceptions   |Disabled (new aborts on oom)
Erase Flash      |Only Sketch
NONOS SDK Version|nonos-sdk 2.2.1+119 (191122)
SSL Support      |Basic SSL ciphers (lower ROM use)
MMU              |32KB cache + 32KB IRAM (balanced)
Non-32-Bit Access|Use pgm_read macros for IRAM/PROGMEM

Then, you can transfer the firmware binary data to ESP-WROOM-02 by any means.

### License

These codes are licensed under [MIT License](LICENSE).
