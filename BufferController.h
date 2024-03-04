#pragma once

#include "common.h"

class BufferController
{
public:
    BufferController();
    void    setup(void);
    void    loop(void);
    ulong   getTargetTime(void);
};
