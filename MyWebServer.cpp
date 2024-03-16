#include "MyWebServer.h"

#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266mDNS.h>
#include <uri/UriBraces.h>
#include <uri/UriRegex.h>

IMPORT_BIN_FILE("index.html", dataIndexHtml);

/*  Defines  */

#define LOOP_INTERVAL       100UL

#define MIMETYPE_TEXT       "text/plain"
#define MIMETYPE_HTML       "text/html"
#define MIMETYPE_GIF        "image/gif"

/*  Local Constants  */

extern const char dataIndexHtml[];

/*  Local Functions  */

static void responseNone(void);
static void responseBadRequest(void);
static void responseForbidden(void);
static void responseNotFound(void);
static void responseTooLarge(void);
static void responseError(void);
static void responseStorageFull(void);

static void handleRoot(void);
static void handleNotFound(void);

static void handleGetArtsList(void);
static void handleUploadArt(void);
static void handleGetArt(void);
static void handleDeleteArt(void);

static void handleDisplayArt(void);
static void handleForwardArt(void);
static void handleFreeze(void);
static void handleDraw(void);
static void handleClear(void);

static void handleGetLeastDuration(void);
static void handlePutLeastDuration(void);
static void handleGetLeastLoop(void);
static void handlePutLeastLoop(void);
static void handleGetActiveDuration(void);
static void handlePutActiveDuration(void);

static void handleGetCurrentArt(void);
static void handleGetPixels(void);
static void handleGetVersion(void);

static uint16_t decodeB64(String b64, uint8_t* pData, uint16_t maxLength);

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
    httpServer.on(F("/"), HTTPMethod::HTTP_GET, handleRoot);
    httpServer.on(F("/api/arts"), HTTPMethod::HTTP_GET, handleGetArtsList);
    httpServer.on(F("/api/arts"), HTTPMethod::HTTP_POST, responseNone, handleUploadArt);
    httpServer.on(UriBraces(F("/api/arts/{}")), HTTPMethod::HTTP_GET, handleGetArt);
    httpServer.on(UriBraces(F("/api/arts/{}")), HTTPMethod::HTTP_DELETE, handleDeleteArt);
    httpServer.on(UriBraces(F("/api/exec/display/{}")), HTTPMethod::HTTP_PUT, handleDisplayArt);
    httpServer.on(F("/api/exec/forward_art"), HTTPMethod::HTTP_PUT, handleForwardArt);
    httpServer.on(F("/api/exec/freeze"), HTTPMethod::HTTP_PUT, handleFreeze);
    httpServer.on(F("/api/exec/draw"), HTTPMethod::HTTP_PUT, handleDraw);
    httpServer.on(F("/api/exec/clear"), HTTPMethod::HTTP_PUT, handleClear);
    httpServer.on(F("/api/configs/least_duration"), HTTPMethod::HTTP_GET, handleGetLeastDuration);
    httpServer.on(F("/api/configs/least_duration"), HTTPMethod::HTTP_PUT, handlePutLeastDuration);
    httpServer.on(F("/api/configs/least_loop"), HTTPMethod::HTTP_GET, handleGetLeastLoop);
    httpServer.on(F("/api/configs/least_loop"), HTTPMethod::HTTP_PUT, handlePutLeastLoop);
    httpServer.on(F("/api/configs/active_duration"), HTTPMethod::HTTP_GET, handleGetActiveDuration);
    httpServer.on(F("/api/configs/active_duration"), HTTPMethod::HTTP_PUT, handlePutActiveDuration);
    httpServer.on(F("/api/status/current_art"), HTTPMethod::HTTP_GET, handleGetCurrentArt);
    httpServer.on(F("/api/status/pixels"), HTTPMethod::HTTP_GET, handleGetPixels);
    httpServer.on(F("/api/version"), HTTPMethod::HTTP_GET, handleGetVersion);
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
    if (isAfter(millis(), targetTime)) {
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

static void responseForbidden(void)
{
    httpServer.send_P(403, PGM_PF(MIMETYPE_TEXT), PGM_PF("403 Forbidden"));
}

static void responseNotFound(void)
{
    httpServer.send_P(404, PGM_PF(MIMETYPE_TEXT), PGM_PF("404 Not Found"));
}

static void responseTooLarge(void)
{
    httpServer.send_P(413, PGM_PF(MIMETYPE_TEXT), PGM_PF("413 Payload Too Large"));
}

static void responseError(void)
{
    httpServer.send_P(500, PGM_PF(MIMETYPE_TEXT), PGM_PF("500 Internal Server Error"));
}

static void responseStorageFull(void)
{
    httpServer.send_P(507, PGM_PF(MIMETYPE_TEXT), PGM_PF("507 Storage Fill"));
}

/*---------------------------------------------------------------------------*/

static void handleRoot(void)
{
    dprintln(F("handleRoot"));
    httpServer.send_P(200, PGM_PF(MIMETYPE_HTML), dataIndexHtml);
}

static void handleNotFound(void)
{
    dprintln(F("handleNotFound"));
    responseNotFound();
}

/*---------------------------------------------------------------------------*/

static void handleGetArtsList(void)
{
    dprintln(F("handleGetArtsList"));
    Dir dir = SPIFFS.openDir(F("/"));
    String ret = "";
    while (dir.next()) {
        ret += dir.fileName().substring(1) + "\n";
    }
    httpServer.send(200, F(MIMETYPE_TEXT), ret);
}

static void handleUploadArt(void)
{
    dprintln(F("handleUploadArt"));
    static File uploadFile;
    HTTPUpload& upload = httpServer.upload();
    if (upload.status == UPLOAD_FILE_START) {
        FSInfo info;
        if (!SPIFFS.info(info)) {
            responseError();
            return;
        }
        if (upload.contentLength > 65536) {
            responseTooLarge();
            return;
        }
        if (info.usedBytes + upload.contentLength + info.blockSize * 2 > info.totalBytes) {
            responseStorageFull();
            return;
        }
        String fsPath = "/" + upload.filename;
        fsPath.toLowerCase();
        uint len = fsPath.length();
        if (len > info.maxPathLength + 4 || !fsPath.endsWith(F(".gif"))) {
            responseBadRequest();
            return;
        }
        len -= 4;
        fsPath = fsPath.substring(0, len);
        for (uint8_t i = 1; i < len; i++) {
            char c = fsPath.charAt(i);
            if (!isalnum(c) && c != '-' && c != '_') {
                responseBadRequest();
                return;
            }
        }
        if (SPIFFS.exists(fsPath)) SPIFFS.remove(fsPath);
        uploadFile = SPIFFS.open(fsPath, "w");
        if (!uploadFile) {
            responseError();
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (uploadFile) {
            size_t bytesWritten = uploadFile.write(upload.buf, upload.currentSize);
            if (bytesWritten != upload.currentSize) {
                uploadFile.close();
                responseError();
            }
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (uploadFile) uploadFile.close();
    }
}

static void handleGetArt(void)
{
    dprintln(F("handleGetArt"));
    String fsPath = "/" + httpServer.pathArg(0);
    if (fsPath.length() > 1 && SPIFFS.exists(fsPath)) {
        File file = SPIFFS.open(fsPath, "r");
        httpServer.streamFile(file, F(MIMETYPE_GIF));
        file.close();
    } else {
        responseNotFound();
    }
}

static void handleDeleteArt(void)
{
    dprintln(F("handleDeleteArt"));
    String fsPath = "/" + httpServer.pathArg(0);
    if (fsPath.length() > 1 && SPIFFS.exists(fsPath)) {
        SPIFFS.remove(fsPath);
        responseNone();
    } else {
        responseNotFound();
    }
}

/*---------------------------------------------------------------------------*/

static void handleDisplayArt(void)
{
    dprintln(F("handleDisplayArt"));
    String name = httpServer.pathArg(0);
    if (controller.displayArtByName(name)) {
        responseNone();
    } else {
        responseNotFound();
    }
}

static void handleForwardArt(void)
{
    dprintln(F("handleForwardArt"));
    controller.forwardArt();
    responseNone();
}

static void handleFreeze(void)
{
    dprintln(F("handleFreeze"));
    controller.freeze();
    responseNone();
}

static void handleDraw(void)
{
    dprintln(F("handleDraw"));
    if (controller.getIsActive()) {
        responseForbidden();
    } else {
        uint8_t data[300];
        uint16_t length = decodeB64(httpServer.arg(F("plain")), data, sizeof(data));
        controller.draw(data, length);
        responseNone();
    }
}

static void handleClear(void)
{
    dprintln(F("handleClear"));
    controller.clear();
    responseNone();
}

/*---------------------------------------------------------------------------*/

static void handleGetLeastDuration(void)
{
    dprintln(F("handleGetLeastDuration"));
    httpServer.send(200, F(MIMETYPE_TEXT), String(controller.getLeastDuration()));
}

static void handlePutLeastDuration(void)
{
    dprintln(F("handlePutLeastDuration"));
    long duration = httpServer.arg(F("plain")).toInt();
    if (duration >= 1 && duration <= 120) {
        controller.setLeastDuration(duration);
        responseNone();
    } else {
        responseBadRequest();
    }
}

static void handleGetLeastLoop(void)
{
    dprintln(F("handleGetLeastLoop"));
    httpServer.send(200, F(MIMETYPE_TEXT), String(controller.getLeastLoop()));
}

static void handlePutLeastLoop(void)
{
    dprintln(F("handlePutLeastLoop"));
    long loop = httpServer.arg(F("plain")).toInt();
    if (loop >= 0 && loop <= 5) {
        controller.setLeastLoop(loop);
        responseNone();
    } else {
        responseBadRequest();
    }
}

static void handleGetActiveDuration(void)
{
    dprintln(F("handleGetActiveDuration"));
    httpServer.send(200, F(MIMETYPE_TEXT), String(controller.getActiveDuration()));
}

static void handlePutActiveDuration(void)
{
    dprintln(F("handlePutActiveDuration"));
    long duration = httpServer.arg(F("plain")).toInt();
    if (duration >= 60 && duration <= 3600) {
        controller.setActiveDuration(duration);
        responseNone();
    } else {
        responseBadRequest();
    }
}

/*---------------------------------------------------------------------------*/

static void handleGetCurrentArt(void)
{
    dprintln(F("handleGetCurrentArt"));
    httpServer.send(200, F(MIMETYPE_TEXT), controller.getCurrentName());
}

static void handleGetPixels(void)
{
    dprintln(F("handleGetPixels"));
    if (controller.getIsActive()) {
        responseForbidden();
    } else {
        uint16_t size;
        uint8_t* pBuffer = controller.getBuffer(size);
        httpServer.send(200, F(MIMETYPE_TEXT), base64::encode(pBuffer, size, false));
    }
}

static void handleGetVersion(void)
{
    dprintln(F("handleGetVersion"));
    httpServer.send_P(200, PGM_PF(MIMETYPE_TEXT), PGM_PF(BUILD_INFO));
}

/*---------------------------------------------------------------------------*/

static uint16_t decodeB64(String b64, uint8_t* pData, uint16_t maxLength)
{
    uint16_t length = 0, value = 0;
    uint8_t bits = 0;
    for (uint16_t i = 0; i < b64.length(); i++) {
        char c = b64.charAt(i);
        int8_t a = -1;
        if (c >='A' && c <='Z') a = c - 'A';
        if (c >='a' && c <='z') a = c - 'a' + 26;
        if (c >='0' && c <='9') a = c - '0' + 52;
        if (c == '+') a = 62;
        if (c == '/') a = 63;
        if (a >= 0) {
            value = value << 6 | a;
            bits += 6;
            if (bits >= 8) {
                pData[length++] = value >> (bits - 8) & 0xFF;
                bits -= 8;
                if (length >= maxLength) break;
            }
        }
    }
    return length;
}
