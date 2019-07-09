#pragma once
#include <initguid.h>
#include <stdbool.h>
#include <stdint.h>

DEFINE_GUID(GUID_DSCapture,
            0xa0b880f6,0xd6a5,0x4700,0xa8,0xea,0x22,0x28,0x2a,0xca,0x55,0x87);

#define DS_CMDIN_STATUS 0x31
#define DS_CMDIN_FRAMEINFO 0x30
#define DS_CMDOUT_CAPTURE_START 0x30
#define DS_CMDOUT_CAPTURE_STOP 0x31
#define DS_INFO_SIZE 64
#define DS_LCD_WIDTH 256
#define DS_LCD_HEIGHT 192
#define DS_FRAME_SIZE 1024 * DS_LCD_HEIGHT

bool win_dscapture_init();
void win_dscapture_deinit();
bool win_dscapture_grabFrame(uint16_t* frameBuffer);
