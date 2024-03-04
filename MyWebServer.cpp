#include "MyWebServer.h"

#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266mDNS.h>

IMPORT_BIN_FILE("index.html", dataIndexHtml);

/*  Defines  */

#define LOOP_INTERVAL       1000

#define MIMETYPE_TEXT       "text/plain"
#define MIMETYPE_HTML       "text/html"

/*  Local Constants  */

extern const char dataIndexHtml[];

/*  Local Functions  */

static void handleRoot(void);
static void handleVersion(void);
static void handleNotFound(void);

/*  Local Functions (Macros)  */

#define PGM_PF(str) (PGM_P)F(str)

/*  External Global Variables  */

extern BufferController controller;

/*  Local Variables  */

static ESP8266WebServer         httpServer(PORT);
static ESP8266HTTPUpdateServer  httpUpdater;

static ulong targetTime;

/*---------------------------------------------------------------------------*/

MyWebServer::MyWebServer()
{
}

void MyWebServer::setup(void)
{
    /*  Setup HTTP Server  */
    httpUpdater.setup(&httpServer);
    httpServer.on(F("/version"), HTTPMethod::HTTP_GET, handleVersion);
    httpServer.on(F("/"), HTTPMethod::HTTP_GET, handleRoot);
    httpServer.onNotFound(handleNotFound);
    httpServer.begin();

    /*  Setup MDNS  */
    MDNS.begin(F(HOSTNAME));
    MDNS.addService(F("http"), F("tcp"), PORT);
    dprintf("http://%s.local:%d/\r\n", HOSTNAME, PORT);

    targetTime = millis();
}

void MyWebServer::loop(void)
{
    if ((long)(targetTime - millis()) <= 0) {
        httpServer.handleClient();
        MDNS.update();
        targetTime += LOOP_INTERVAL;
    }
}

ulong MyWebServer::getTargetTime(void)
{
    return targetTime;
}

/*---------------------------------------------------------------------------*/

static void responseNone(void)
{
    httpServer.send(204);
}

static void responseBadRequest(void)
{
    httpServer.send_P(400, PGM_PF(MIMETYPE_TEXT), PGM_PF("400 Bad Request"));
}

static void responseNotFound(void)
{
    httpServer.send_P(404, PGM_PF(MIMETYPE_TEXT), PGM_PF("404 Not Found"));
}

static void responseError(void)
{
    httpServer.send_P(500, PGM_PF(MIMETYPE_TEXT), PGM_PF("500 Internal Server Error"));
}

/*---------------------------------------------------------------------------*/

static void handleRoot(void)
{
    dprintln(F("handleRoot"));
    httpServer.send_P(200, PGM_PF(MIMETYPE_HTML), dataIndexHtml);
}

static void handleVersion(void)
{
    dprintln(F("handleVersion"));
    httpServer.send_P(200, PGM_PF(MIMETYPE_TEXT), PGM_PF(BUILD_INFO));
}

static void handleNotFound(void)
{
    dprintln(F("handleNotFound"));
    responseNotFound();
}
