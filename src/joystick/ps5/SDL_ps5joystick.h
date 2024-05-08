#pragma once

#include <stdint.h>

#define PS5_PAD_BUTTON_L3        0x0002
#define PS5_PAD_BUTTON_R3        0x0004
#define PS5_PAD_BUTTON_OPTIONS   0x0008
#define PS5_PAD_BUTTON_UP        0x0010
#define PS5_PAD_BUTTON_RIGHT     0x0020
#define PS5_PAD_BUTTON_DOWN      0x0040
#define PS5_PAD_BUTTON_LEFT      0x0080
#define PS5_PAD_BUTTON_L2        0x0100
#define PS5_PAD_BUTTON_R2        0x0200
#define PS5_PAD_BUTTON_L1        0x0400
#define PS5_PAD_BUTTON_R1        0x0800
#define PS5_PAD_BUTTON_TRIANGLE  0x1000
#define PS5_PAD_BUTTON_CIRCLE    0x2000
#define PS5_PAD_BUTTON_CROSS     0x4000
#define PS5_PAD_BUTTON_SQUARE    0x8000
#define PS5_PAD_BUTTON_TOUCH_PAD 0x100000

typedef struct PS5_PadTouch
{
    uint16_t x;
    uint16_t y;
    uint8_t finger;
    uint8_t pad[3];
} PS5_PadTouch;

typedef struct PS5_PadTouchData
{
    uint8_t fingers;
    uint8_t pad1[3];
    uint32_t pad2;
    PS5_PadTouch touch[2];
} PS5_PadTouchData;

typedef struct PS5_PadData
{
    uint32_t buttons;
    struct
    {
        uint8_t x;
        uint8_t y;
    } leftStick;
    struct
    {
        uint8_t x;
        uint8_t y;
    } rightStick;
    struct
    {
        uint8_t l2;
        uint8_t r2;
    } analogButtons;
    uint16_t padding;
    struct
    {
        float x;
        float y;
        float z;
        float w;
    } quat;
    struct
    {
        float x;
        float y;
        float z;
    } vel;
    struct
    {
        float x;
        float y;
        float z;
    } acell;
    PS5_PadTouchData touch;
    uint8_t connected;
    uint64_t timestamp;
    uint8_t ext[16];
    uint8_t count;
    uint8_t unknown[15];
} PS5_PadData;

int scePadInit(void);
int scePadOpen(int handle, int, int, void *);
int scePadReadState(int handle, PS5_PadData *data);
int scePadClose(int handle);

int sceUserServiceInitialize(void *);
int sceUserServiceGetLoginUserIdList(int userId[4]);
