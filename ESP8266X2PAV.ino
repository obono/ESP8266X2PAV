#include "common.h"

#include <ESP8266WiFi.h>

/*  Defines  */

#define BUTTON_PIN  4

/*  Global Variables  */

BufferController    controller;
MyWebServer         server;

/*  Local Variables  */

static int  lastButtonState;

/*----------------------------------------------------------------------------*/

void setup(void)
{
#ifdef DEBUG
    Serial.begin(74880);
#endif
    dprintln();
    dprintln(F("### ESP8266X2PAV ###"));
    dprintln(F("Version: " BUILD_INFO));
    pinMode(BUTTON_PIN, INPUT);
    lastButtonState = digitalRead(BUTTON_PIN);
    SPIFFS.begin();
    controller.setup();

    /*  Connect to Access Point  */
    if (lastButtonState == HIGH) { // not pressed
        WiFi.mode(WIFI_STA);
#ifdef STATIC_ADDRESS
        IPAddress staticIP(STATIC_ADDRESS_IP);
        IPAddress gateway(STATIC_ADDRESS_GATEWAY);
        IPAddress subnet(STATIC_ADDRESS_SUBNET);
        IPAddress dns(STATIC_ADDRESS_DNS);
        WiFi.config(staticIP, subnet, gateway, dns);
#endif
        WiFi.begin(STA_SSID, STA_PASSWORD);
        while (WiFi.waitForConnectResult() != WL_CONNECTED) {
            dprintln(F("WiFi failed, retrying."));
            delay(5000);
            WiFi.begin(STA_SSID, STA_PASSWORD);
        }
        wifi_set_sleep_type(LIGHT_SLEEP_T);
        dprint(F("IP address: "));
        dprintln(WiFi.localIP());
        dprint(F("Mac address: "));
        dprintln(WiFi.macAddress());
        server.setup();
    } else {
        dprintln(F("Stand alone mode."));
    }
}

void loop(void)
{
    controller.loop();
    server.loop();
    int buttonState = digitalRead(BUTTON_PIN);
    if (buttonState != lastButtonState) {
        if (buttonState == LOW) {
            controller.forwardArt();
        }
        lastButtonState = buttonState;
    }

    /*  Wait  */
    ulong now = millis();
    long waitForController = controller.getTargetTime() - now;
    long waitForServer = server.getTargetTime() - now;
    long wait = min(waitForController, waitForServer);
    if (wait > 0) delay(wait);
}
