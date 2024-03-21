#include "BufferController.h"
#include "gifdec.h"

#include <SPI.h>

/*---------------------------------------------------------------------------*/

class UnicornHatHD
{
public:
    UnicornHatHD(uint8_t width_, uint8_t height_) :
        mySPISettings(9000000, MSBFIRST, SPI_MODE0), width(width_), height(height_)
    {
    }

    void init(void)
    {
        pinMode(SPI_CS_PIN, OUTPUT);
        digitalWrite(SPI_CS_PIN, HIGH);
        SPI.begin();
    }

    void transferFrameBuffer(uint8_t *pBuffer)
    {
        SPI.beginTransaction(mySPISettings);
        digitalWrite(SPI_CS_PIN, LOW);
        SPI.transfer(COMMAND_SOF);
        SPI.transferBytes(pBuffer, NULL, width * height * 3);
        digitalWrite(SPI_CS_PIN, HIGH);
        SPI.endTransaction();
    }

    void screenOff(void)
    {
        SPI.beginTransaction(mySPISettings);
        digitalWrite(SPI_CS_PIN, LOW);
        SPI.transfer(COMMAND_SOF);
        for (int i = 0; i < width * height * 3; i++) {
            SPI.transfer(0);
        }
        digitalWrite(SPI_CS_PIN, HIGH);
        SPI.endTransaction();
    }

    void finish(void)
    {
        SPI.end();
    }

private:
    static const uint8_t    SPI_CS_PIN = 5;
    static const uint8_t    COMMAND_SOF = 0x72;

    const SPISettings   mySPISettings;
    const uint8_t       width, height;
};

/*---------------------------------------------------------------------------*/

/*  Defines  */

/*  Typedefs  */
typedef struct {
    uint16_t    leastDuration;
    uint16_t    leastLoop;
    uint16_t    activeDuration;
} CONFIG_T;

/*  Local Functions  */

static void wakeUp(bool isRefresh);
static void sleep(void);
static void openFile(String &name);
static void closeFile(void);
static void openByIndex(uint16_t index);
static void updateFrame(void);

/*  Local Variables  */

static UnicornHatHD hat(PIXELS_NUM, PIXELS_NUM);
static gd_GIF*      pGif;
static CONFIG_T     config;

static String   currentName;
static ulong    targetFrameTime, targetLeastTime, targetActiveTime;
static uint16_t currentIndex, currentFrame, currentLoop;
static uint8_t  buffer[PIXELS_NUM * PIXELS_NUM * 3];
static bool     isActive, isSequencial, isSleep;

/*---------------------------------------------------------------------------*/

BufferController::BufferController()
{
}

void BufferController::setup(void)
{
    pGif = NULL;
    config.leastDuration = 15;
    config.leastLoop = 2;
    config.activeDuration = 300;

    isSleep = true;
    wakeUp(false);
    openByIndex(0);
}

void BufferController::loop(void)
{
    if (isSleep) return;

    ulong now = millis();
    if (isAfter(now, targetActiveTime)) {
        sleep();
    } else if (isActive) {
        if (isAfter(now, targetFrameTime)) updateFrame();
        if (isSequencial && isAfter(now, targetLeastTime) && currentLoop >= config.leastLoop) {
            openByIndex(currentIndex + 1);
        } 
    }
}

ulong BufferController::getTargetTime(void)
{
    if (isSleep) return millis() + 1000UL; // TODO

    if (isAfter(targetActiveTime, targetLeastTime)) {
        return (isAfter(targetFrameTime, targetLeastTime)) ? targetLeastTime : targetFrameTime;
    } else {
        return (isAfter(targetFrameTime, targetActiveTime)) ? targetActiveTime : targetFrameTime;
    }
}

/*---------------------------------------------------------------------------*/

bool BufferController::displayArtByName(String& name)
{
    if (name.length() > 0 && SPIFFS.exists("/" + name)) {
        wakeUp(false);
        isSequencial = false;
        openFile(name);
        return true;
    } else {
        return false;
    }
}

void BufferController::forwardArt(void)
{
    if (!isSleep && isActive && isSequencial) currentIndex++;
    wakeUp(false);
    openByIndex(currentIndex);
}

void BufferController::freeze(void)
{
    isActive = false;
}

void BufferController::draw(uint8_t *pData, uint16_t size)
{
    if (isActive || size < 3) return;
    wakeUp(false);
    for (uint16_t i = 3; i < size; i++) {
        memcpy(&buffer[pData[i] * 3], pData, 3);
    }
    hat.transferFrameBuffer(buffer);
}

void BufferController::clear(void)
{
    wakeUp(false);
    isActive = false;
    memset(buffer, 128, sizeof(buffer));
    hat.transferFrameBuffer(buffer);
}

/*---------------------------------------------------------------------------*/

uint16_t BufferController::getLeastDuration(void)
{
    return config.leastDuration;
}

void BufferController::setLeastDuration(uint16_t duration)
{
    dprint(F("#least duration = "));
    dprintln(duration);
    long diff = (duration - config.leastDuration) * 1000U;
    targetLeastTime += diff;
    config.leastDuration = duration;
}

uint16_t BufferController::getLeastLoop(void)
{
    return config.leastLoop;
}

void BufferController::setLeastLoop(uint16_t loop)
{
    dprint(F("#least loop = "));
    dprintln(loop);
    config.leastLoop = loop;
}

uint16_t BufferController::getActiveDuration(void)
{
    return config.activeDuration;
}

void BufferController::setActiveDuration(uint16_t duration)
{
    dprint(F("#active duration = "));
    dprintln(duration);
    long diff = (duration - config.activeDuration) * 1000U;
    targetActiveTime += diff;
    config.activeDuration = duration;
}

/*---------------------------------------------------------------------------*/

bool BufferController::getIsActive(void)
{
    return isActive;
}

String& BufferController::getCurrentName(void)
{
    return currentName;
}

uint8_t* BufferController::getBuffer(uint16_t& size)
{
    size = sizeof(buffer);
    return buffer;
}

/*---------------------------------------------------------------------------*/

static void wakeUp(bool isRefresh)
{
    dprintln(F("#wake up"));
    if (isSleep) {
        hat.init();
        if (isRefresh) hat.transferFrameBuffer(buffer);
        isSleep = false;
    }
    isSleep = false;
    targetActiveTime = millis() + config.activeDuration * 1000UL;
}

static void sleep(void)
{
    dprintln(F("#sleep"));
    hat.screenOff();
    hat.finish();
    isSleep = true;
}

static void openFile(String& name)
{
    dprint(F("#open "));
    dprintln(name);
    closeFile(); 
    pGif = gd_open_gif(("/" + name).c_str());
    if (!pGif) {
        dprintln(F("#failed!"));
        currentName = "";
        isActive = false;
        isSleep = false;
        targetFrameTime = targetActiveTime;
        // TODO: Indicate failed
        return;
    }

    currentName = name;
    currentFrame = 0;
    currentLoop = 0;
    isActive = true;
    isSleep = false;
    targetLeastTime = millis() + config.leastDuration * 1000UL;
    updateFrame();
}

static void closeFile(void)
{
    if (pGif) {
        dprintln(F("#close"));
        gd_close_gif(pGif);
        pGif = NULL;
    }
}

static void openByIndex(uint16_t index)
{
    Dir dir = SPIFFS.openDir(F("/"));
    uint16_t counter = 0;
    bool isEmpty = true;
    while (dir.next()) {
        isEmpty = false;
        if (counter == 0 || counter == index) {
            currentName = dir.fileName().substring(1);
            currentIndex = counter;
            if (counter == index) break;
        }
        counter++;
    }
    if (isEmpty) {
        dprintln(F("#empty"));
        currentName = "";
        currentIndex = 0;
        isActive = false;
        isSleep = false;
        // TODO: Indicate empty
        return;
    }

    dprint(F("#index = "));
    dprintln(currentIndex);
    isSequencial = true;
    openFile(currentName);
}

static void updateFrame(void)
{
    if (pGif) {
        while (!gd_get_frame(pGif)) {
            currentLoop++;
            if (currentLoop != pGif->loop_count) {
                dprintln(F("#rewind"));
                gd_rewind(pGif);
            } else {
                closeFile();
                if (currentLoop < config.leastLoop) currentLoop = config.leastLoop;
                targetFrameTime = (isSequencial) ? targetLeastTime : targetActiveTime;
                return;
            }
        }
        uint8_t work[pGif->width * pGif->height * 3];
        uint8_t *p = buffer, *pBGColor = &pGif->palette->colors[pGif->bgindex * 3];
        int8_t xOffset = (pGif->width - PIXELS_NUM) / 2, yOffset = (pGif->height - PIXELS_NUM) / 2;
        gd_render_frame(pGif, work);
        for (int8_t y = yOffset; y < PIXELS_NUM + yOffset; y++) {
            for (int8_t x = xOffset; x < PIXELS_NUM + xOffset; x++) {
                if (x >= 0 && y >= 0 && x < pGif->width && y < pGif->height) {
                    memcpy(p, &work[(y * pGif->width + x) * 3], 3);
                } else {
                    memcpy(p, pBGColor, 3);
                }
                p += 3;
            }
        }
        hat.transferFrameBuffer(buffer);
        targetFrameTime = millis() + pGif->gce.delay * 10UL;
    } else {
        dprintln(F("#nop"));
        targetFrameTime = (isSequencial) ? targetLeastTime : targetActiveTime;
    }
}
