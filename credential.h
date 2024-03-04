#pragma once

#define HOSTNAME        "esp8266x2pav"
#define PORT            8080

#define STA_SSID        "your-ssid"
#define STA_PASSWORD    "your-password"
//#define STATIC_ADDRESS

#ifdef STATIC_ADDRESS
#define octets(a, b, c, d)      (uint32_t)((a) | (b) << 8 | (c) << 16 | (d) << 24)
#define STATIC_ADDRESS_IP       octets(192, 168, 0, 100)
#define STATIC_ADDRESS_GATEWAY  octets(192, 168, 0, 1)
#define STATIC_ADDRESS_SUBNET   octets(255, 255, 255, 0)
#define STATIC_ADDRESS_DNS      octets(192, 168, 0, 1)
#endif
