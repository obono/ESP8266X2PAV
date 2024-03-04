#pragma once

#include "common.h"

class MyWebServer
{
public:
    MyWebServer();
    void    setup(void);
    void    loop(void);
    ulong   getTargetTime(void);
};
