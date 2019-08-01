#pragma once
#include <initguid.h>
#include <stdbool.h>
#include <stdint.h>
#include <Windows.h>
#include <strsafe.h>
#include <winusb.h>
#include <winnt.h>
#include <cfgmgr32.h>
#include <atomic>
#include <mutex>

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
#define DS_BUFFER_SIZE 8
#define DS_THREADS 1

class DSCapture
{
public:
    DSCapture();
    ~DSCapture();
    void grabFrame(uint16_t* frameBuffer);
    void startCapture();
    void endCapture();

private:
    bool handlesOpen;
    WINUSB_INTERFACE_HANDLE winusbHandle;
    HANDLE deviceHandle;
    char devicePath[260];
    unsigned char bulkPipeInId;
    unsigned short maxPacketSize;

    uint16_t* frameBuffer;
    uint8_t* frameInfoBuffer;

    std::atomic_bool doCapture;
    std::atomic_int framesInBuffer;
    std::thread captureThreads[DS_THREADS];

    HRESULT openDevice();
    void closeDevice();
    void captureFrame();
    HRESULT retrieveDevicePath(char* path, ULONG buflen);
    bool queryDeviceEndpoints();
    bool sendToDefaultEndpoint(uint8_t request, uint16_t value, uint16_t length, uint8_t* buf);
    bool recvFromDefaultEndpoint(uint8_t request, uint16_t length, uint8_t* buf);
    bool readFromBulk(uint8_t* buf, int length, PULONG transferred);
    bool getMaximumTransferSize(unsigned long* mts);
};
