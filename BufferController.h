#pragma once

#include "common.h"

#include <WString.h>

class BufferController
{
public:
    BufferController();
    void    setup(void);
    void    loop(void);
    ulong   getTargetTime(void);
    bool    displayArtByName(String& name);
    void    forwardArt(void);
    void    freeze(void);
    void    draw(uint8_t* pData, uint16_t size);
    void    clear(void);
    uint16_t getLeastDuration(void);
    void    setLeastDuration(uint16_t duration);
    uint16_t getLeastLoop(void);
    void    setLeastLoop(uint16_t loop);
    uint16_t getActiveDuration(void);
    void    setActiveDuration(uint16_t duration);
    bool    getIsActive(void);
    String& getCurrentName(void);
    uint8_t* getBuffer(uint16_t& size);
};
