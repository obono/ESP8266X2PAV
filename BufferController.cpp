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

/*  Local Variables  */

static UnicornHatHD hat(16, 16);

/*---------------------------------------------------------------------------*/

BufferController::BufferController()
{
}

void BufferController::setup(void)
{
    hat.init();
}

void BufferController::loop(void)
{
}

ulong BufferController::getTargetTime(void)
{
    return 0xFFFFFFFFUL;
}

/*---------------------------------------------------------------------------*/
