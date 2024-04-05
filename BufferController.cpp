#include "BufferController.h"
#include "gifdec.h"

#include <EEPROM.h>
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

#define SLEEPING_LOOP_INTERVAL  500UL

/*  Typedefs  */
typedef struct {
    uint16_t    leastDuration;
    uint16_t    leastLoop;
    uint16_t    activeDuration;
} CONFIG_T;

/*  Local Functions  */

static void readConfig(void);
static void writeConfig(void);
static void wakeUp(bool isRefresh);
static void sleep(void);
static void openFile(String &name);
static void closeFile(void);
static void openByIndex(uint16_t index);
static void updateFrame(void);
static void displayConstBitmap(const uint16_t *pBitmap, uint8_t r, uint8_t g, uint8_t b);

/*  Local Constants  */

static uint16_t bitmapBooting[] = {
    0x0000, 0x18C7, 0x2529, 0x2527, 0x2529, 0x2529, 0x18C7, 0x0000,
    0x64AF, 0x95A2, 0x16A2, 0xD4A2, 0x94A2, 0x64A2, 0x0000, 0x0000,
};

static uint16_t bitmapError[] = {
    0x0000, 0x1CEF, 0x2521, 0x2527, 0x1CE1, 0x14A1, 0x252F, 0x0000,
    0x7300, 0x9480, 0x9480, 0x7480, 0x5480, 0x9300, 0x0000, 0x0000,
};

static uint16_t bitmapNoFile[] = {
    0x00C9, 0x012B, 0x012D, 0x0129, 0x0129, 0x00C9, 0x0000, 0x0000,
    0x0000, 0xF0AF, 0x10A1, 0x70A7, 0x10A1, 0x10A1, 0xF7A1, 0x0000,
};

/*  Local Variables  */

static UnicornHatHD hat(PIXELS_NUM, PIXELS_NUM);
static gd_GIF       *pGif;
static CONFIG_T     config;

static String   currentName;
static ulong    targetFrameTime, targetLeastTime, targetActiveTime;
static uint16_t currentIndex, currentLoop;
static uint8_t  buffer[PIXELS_NUM * PIXELS_NUM * 3];
static bool     isActive, isSequencial, isSleep, isConfigDirty;

/*---------------------------------------------------------------------------*/

BufferController::BufferController()
{
}

void BufferController::setup(void)
{
    readConfig();
    pGif = NULL;
    currentIndex = 0;
    isSleep = true;
    isActive = false;
    wakeUp(false);
    displayConstBitmap(bitmapBooting, 0, 127, 255);
    hat.transferFrameBuffer(buffer);
}

void BufferController::loop(void)
{
    if (isSleep) return;

    ulong now = millis();
    if (isAfter(now, targetActiveTime)) {
        writeConfig();
        sleep();
    } else if (isActive) {
        if (isAfter(now, targetFrameTime)) updateFrame();
        if (isSequencial && isAfter(now, targetLeastTime) && currentLoop >= config.leastLoop) {
            writeConfig();
            openByIndex(currentIndex + 1);
        } 
    }
}

ulong BufferController::getTargetTime(void)
{
    if (isSleep) return millis() + SLEEPING_LOOP_INTERVAL;

    if (isAfter(targetActiveTime, targetLeastTime)) {
        return (isAfter(targetFrameTime, targetLeastTime)) ? targetLeastTime : targetFrameTime;
    } else {
        return (isAfter(targetFrameTime, targetActiveTime)) ? targetActiveTime : targetFrameTime;
    }
}

/*---------------------------------------------------------------------------*/

bool BufferController::displayArtByName(String& name)
{
    writeConfig();
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
    writeConfig();
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
    if (diff) {
        targetLeastTime += diff;
        config.leastDuration = duration;
        isConfigDirty = true;
    }
}

uint16_t BufferController::getLeastLoop(void)
{
    return config.leastLoop;
}

void BufferController::setLeastLoop(uint16_t loop)
{
    dprint(F("#least loop = "));
    dprintln(loop);
    if (config.leastLoop != loop) {
        config.leastLoop = loop;
        isConfigDirty = true;
    }
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
    if (diff) {
        targetActiveTime += diff;
        config.activeDuration = duration;
        isConfigDirty = true;
    }
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

static void readConfig(void)
{
    EEPROM.begin(sizeof(config));
    EEPROM.get(0, config);
    if (config.leastDuration < CONFIG_LEAST_DURATION_MIN || config.leastDuration > CONFIG_LEAST_DURATION_MAX) {
        config.leastDuration = CONFIG_LEAST_DURATION_DEFAULT;
    }
    if (config.leastLoop < CONFIG_LEAST_LOOP_MIN || config.leastLoop > CONFIG_LEAST_LOOP_MAX) {
        config.leastLoop = CONFIG_LEAST_LOOP_DEFAULT;
    }
    if (config.activeDuration < CONFIG_ACTIVE_DURATION_MIN || config.activeDuration > CONFIG_ACTIVE_DURATION_MAX) {
        config.activeDuration = CONFIG_ACTIVE_DURATION_DEFAULT;
    }
    isConfigDirty = false;
}

static void writeConfig(void)
{
    if (isConfigDirty) {
        EEPROM.put(0, config);
        EEPROM.commit();
        isConfigDirty = false;
    }
}

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
    closeFile(); 
    dprint(F("#open "));
    dprintln(name);
    pGif = gd_open_gif(("/" + name).c_str());
    if (!pGif) {
        dprintln(F("#failed!"));
        displayConstBitmap(bitmapError, 255, 31, 127);
    }
    currentName = name;
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
        displayConstBitmap(bitmapNoFile, 255, 191, 95);
        hat.transferFrameBuffer(buffer);
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
        int8_t ret;
        while ((ret = gd_get_frame(pGif)) != 1) {
            if (ret == -1) {
                dprintln(F("#frame error!"));
                closeFile();
                displayConstBitmap(bitmapError, 255, 63, 0);
                hat.transferFrameBuffer(buffer);
                goto finished;
            } else if (ret == 0) {
                currentLoop++;
                if (pGif->loop_count == 0 || currentLoop < pGif->loop_count) {
                    dprintln(F("#rewind"));
                    gd_rewind(pGif);
                } else {
                    dprintln(F("#finished"));
                    closeFile();
                    goto finished;
                }
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
        return; // usual case
    }

    dprintln(F("#nop"));
finished:
    currentLoop = UINT16_MAX;
    targetFrameTime = (isSequencial) ? targetLeastTime : targetActiveTime;
}

static void displayConstBitmap(const uint16_t *pBitmap, uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t color[3] =  { r,      g,      b      };
    uint8_t forth[3] =  { r >> 2, g >> 2, b >> 2 };
    for (uint16_t i = 0; i < PIXELS_NUM * PIXELS_NUM; i++) {
        memcpy(&buffer[i * 3], forth, 3);
    }
    for (uint8_t y = 0; y < PIXELS_NUM; y++) {
        uint16_t pattern = pgm_read_word(&pBitmap[y]);
        for (uint8_t x = 0; x < PIXELS_NUM; x++) {
            if (bitRead(pattern, x)) {
                memcpy(&buffer[(y * PIXELS_NUM + x) * 3], color, 3);
                if (y < PIXELS_NUM - 1) memset(&buffer[((y + 1) * PIXELS_NUM + x) * 3], 0, 3);
            }
        }
    }
    hat.transferFrameBuffer(buffer);
}