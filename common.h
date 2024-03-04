#pragma once

#include <Arduino.h>
#include <FS.h>

#include "credential.h"
#include "BufferController.h"
#include "MyWebServer.h"

/*  Defines  */

#define DEBUG

#define BUILD_VERSION   "0.0.1"
#define BUILD_DATETIME  __DATE__ " " __TIME__
#ifdef DEBUG
#define BUILD_INFO      BUILD_VERSION "-debug (" BUILD_DATETIME ")"
#else
#define BUILD_INFO      BUILD_VERSION "(" BUILD_DATETIME ")"
#endif

/*  Typedefs  */

typedef unsigned long ulong;

/*  Global Functions (Macros)  */

#define IMPORT_BIN_FILE(file, sym) asm (    \
    ".global " #sym "\n"                    \
    #sym ":\n"                              \
    ".incbin \"" file "\"\n"                \
    "nop\n"                                 \
    ".set _sizeof_" #sym ", . - " #sym "\n" \
    ".balign 4\n")

#ifdef DEBUG
#define dprint(...)     Serial.print(__VA_ARGS__)
#define dprintln(...)   Serial.println(__VA_ARGS__)
#define dprintf(...)    Serial.printf(__VA_ARGS__)
#else
#define dprint(...)
#define dprintln(...)
#define dprintf(...)
#endif
