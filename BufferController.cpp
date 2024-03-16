#include "BufferController.h"

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

    void transferFrameBuffer(uint8_t *frameBuffer, uint8_t brightness)
    {
        if (brightness > BRIGHTNESS_MAX) brightness = BRIGHTNESS_MAX;
        SPI.beginTransaction(mySPISettings);
        digitalWrite(SPI_CS_PIN, LOW);
        SPI.transfer(COMMAND_SOF);
        for (uint8_t *p = frameBuffer; p < frameBuffer + width * height * 3; p++) {
            SPI.transfer(*p * brightness / BRIGHTNESS_MAX);
        }
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
    static const uint8_t    BRIGHTNESS_MAX = 100;

    const SPISettings   mySPISettings;
    const uint8_t       width, height;
};

/*---------------------------------------------------------------------------*/

/*  Defines  */

#define PIXELS_NUM      16
#define LOOP_INTERVAL   200UL

/*  Typedefs  */
typedef struct {
    uint16_t    leastDuration;
    uint16_t    leastLoop;
    uint16_t    activeDuration;
} CONFIG_T;

/*  Local Functions  */

static void wakeUp(bool isRefresh);
static void sleep(void);
static void openFile(String& name);
static void openByIndex(uint16_t index);
static void updateFrame(void);

/*  Local Variables  */

static UnicornHatHD hat(PIXELS_NUM, PIXELS_NUM);
static CONFIG_T     config;

static String   currentName;
static ulong    targetFrameTime, targetLeastTime, targetActiveTime;
static uint16_t currentIndex, currentFrame, currentLoop;
static uint8_t  buffer[PIXELS_NUM * PIXELS_NUM * 3];
static uint8_t  tempR, tempG, tempB;
static bool     isActive, isSequencial, isSleep;

/*---------------------------------------------------------------------------*/

BufferController::BufferController()
{
}

void BufferController::setup(void)
{
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
        if (isAfter(now, targetFrameTime)) {
            if (++currentFrame >= 32) {
                currentFrame = 0;
                currentLoop++;
            }
            updateFrame();
            targetFrameTime = now + LOOP_INTERVAL;
        }
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

void BufferController::draw(uint8_t* pData, uint16_t size)
{
    if (isActive || size < 3) return;
    wakeUp(false);
    for (uint16_t i = 3; i < size; i++) {
        memcpy(&buffer[pData[i] * 3], pData, 3);
    }
    hat.transferFrameBuffer(buffer, 100);
}

void BufferController::clear(void)
{
    wakeUp(false);
    isActive = false;
    memset(buffer, 128, sizeof(buffer));
    hat.transferFrameBuffer(buffer, 100);
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
        if (isRefresh) hat.transferFrameBuffer(buffer, 100);
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
    // TODO: Open the pixel art
    currentName = name;
    currentFrame = 0;
    currentLoop = 0;
    isActive = true;
    isSleep = false;
    updateFrame();
    targetFrameTime = millis() + LOOP_INTERVAL;
    targetLeastTime = millis() + config.leastDuration * 1000UL;
    tempR = random(9);
    tempG = random(9);
    tempB = random(9);
}

static void openByIndex(uint16_t index)
{
    Dir dir = SPIFFS.openDir(F("/"));
    uint16_t counter = 0;
    bool isEmpty = true;
    while (dir.next()) {
        isEmpty = false;
        if (counter == 0 || counter == index) {
            currentName = dir.fileName();
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
    } else {
        dprint(F("#index = "));
        dprintln(currentIndex);
        isSequencial = true;
        openFile(currentName);
    }
}

static void updateFrame(void)
{
    for (uint16_t i = 0; i < PIXELS_NUM * PIXELS_NUM; i++) {
        buffer[i * 3]     = tempR * currentFrame + random(8);
        buffer[i * 3 + 1] = tempG * currentFrame + random(8);
        buffer[i * 3 + 2] = tempB * currentFrame + random(8);
    }
    hat.transferFrameBuffer(buffer, 100);
}
