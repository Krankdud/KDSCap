#pragma once
#include <initguid.h>
#include <stdbool.h>

DEFINE_GUID(GUID_DSCapture,
            0xa0b880f6,0xd6a5,0x4700,0xa8,0xea,0x22,0x28,0x2a,0xca,0x55,0x87);

bool win_dscapture_init();
void win_dscapture_deinit();

