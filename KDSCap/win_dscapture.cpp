#include <stdexcept>
#include <thread>
#include "win_dscapture.h"

typedef struct _UsbDeviceRequest
{
    union
    {
        BYTE requestType;
        struct
        {
            BYTE recipient : 5;
            BYTE type : 2;
            BYTE dataDirection : 1;
        } bits;
    };
} UsbDeviceRequest;

DSCapture::DSCapture()
{
    USB_DEVICE_DESCRIPTOR deviceDesc;
    ULONG lengthReceived;

    HRESULT hr = openDevice();
    if (FAILED(hr))
    {
        printf("Failed looking for device, HRESULT 0x%x\n", hr);
        throw std::runtime_error("Could not open device");
    }

    bool result = WinUsb_GetDescriptor(winusbHandle,
                                       USB_DEVICE_DESCRIPTOR_TYPE,
                                       0,
                                       0,
                                       ( PBYTE) & deviceDesc,
                                       sizeof(deviceDesc),
                                       &lengthReceived);
    if (result == false || lengthReceived != sizeof(deviceDesc))
    {
        printf("Error among LastError %d or lengthReceived %d\n", result == false ? GetLastError() : 0, lengthReceived);
        closeDevice();
        throw std::runtime_error("Could not open device");
    }

    printf("Device found: VID_%04X&PID_%04X\n", deviceDesc.idVendor, deviceDesc.idProduct);

    if (!queryDeviceEndpoints())
    {
        printf("Couldn't query device endpoints\n");
        closeDevice();
        throw std::runtime_error("Could not query device endpoints");
    }

    bool rawIO = true;
    if (!WinUsb_SetPipePolicy(winusbHandle, bulkPipeInId, RAW_IO, sizeof(bool), &rawIO))
    {
        printf("Couldn't set pipe policy: %d\n", GetLastError());
        closeDevice();
        throw std::runtime_error("Couldn't set pipe policy");
    }
}

DSCapture::~DSCapture()
{
    closeDevice();
}

bool DSCapture::grabFrame(uint16_t* frameBuffer)
{
    static uint16_t tmpBuf[DS_FRAME_SIZE / sizeof(uint16_t)];
    static uint8_t frameInfo[DS_INFO_SIZE];

    uint8_t dummy;
    if (!sendToDefaultEndpoint(DS_CMDOUT_CAPTURE_START, 0, 0, &dummy))
    {
        return false;
    }

    ULONG transferred;
    bool result;
    int bytesIn = 0;
    uint8_t* p = ( uint8_t*) tmpBuf;
    do
    {
        result = readFromBulk(p, DS_FRAME_SIZE - bytesIn, &transferred);
        if (result)
        {
            bytesIn += transferred;
            p += transferred;
        }
    } while (bytesIn < DS_FRAME_SIZE && result && transferred > 0);

    if (!result)
        return false;
    if (!recvFromDefaultEndpoint(DS_CMDIN_FRAMEINFO, DS_INFO_SIZE, frameInfo))
        return false;
    if ((frameInfo[0] & 3) != 3)
        return false;
    if (!frameInfo[52])
        return false;

    uint16_t* src = tmpBuf;
    uint16_t* dst = frameBuffer;
    for (int line = 0; line < DS_LCD_HEIGHT * 2; ++line)
    {
        if (frameInfo[line >> 3] & (1 << (line & 7)))
        {
            for (int i = 0; i < DS_LCD_WIDTH / 2; ++i)
            {
                dst[0] = src[1];
                dst[DS_LCD_WIDTH * DS_LCD_HEIGHT] = src[0];
                dst++;
                src += 2;
            }
        }
        else
        {
            memcpy(dst, dst - 256, 256);
            memcpy(dst + 256 * 192, dst + 256 * 191, 256);
            dst += 128;
        }
    }

    return true;
}

void DSCapture::startCapture(uint16_t* frameBuffer, std::mutex* mutex)
{
    doCapture = true;
    for (int i = 0; i < DS_THREADS; ++i)
    {
        captureThreads[i] = std::thread(&DSCapture::captureFrame, this, frameBuffer, mutex);
    }
}

void DSCapture::endCapture()
{
    doCapture = false;
    for (int i = 0; i < DS_THREADS; ++i)
    {
        captureThreads[i].join();
    }
}

void DSCapture::captureFrame(uint16_t* frameBuffer, std::mutex* mutex)
{
    while (doCapture)
    {
        static uint16_t tmpBuf[DS_FRAME_SIZE / sizeof(uint16_t)];
        static uint8_t frameInfo[DS_INFO_SIZE];

        uint8_t dummy;
        if (!sendToDefaultEndpoint(DS_CMDOUT_CAPTURE_START, 0, 0, &dummy))
        {
            return;
        }

        ULONG transferred;
        bool result;
        int bytesIn = 0;
        uint8_t* p = ( uint8_t*) tmpBuf;
        do
        {
            result = readFromBulk(p, DS_FRAME_SIZE - bytesIn, &transferred);
            if (result)
            {
                bytesIn += transferred;
                p += transferred;
            }
        } while (bytesIn < DS_FRAME_SIZE && result && transferred > 0);

        if (!result)
            return;
        if (!recvFromDefaultEndpoint(DS_CMDIN_FRAMEINFO, DS_INFO_SIZE, frameInfo))
            return;
        if ((frameInfo[0] & 3) != 3)
            return;
        if (!frameInfo[52])
            return;

        uint16_t* src = tmpBuf;
        uint16_t* dst = frameBuffer;
        {
            std::lock_guard<std::mutex> lock(*mutex);
            for (int line = 0; line < DS_LCD_HEIGHT * 2; ++line)
            {
                if (frameInfo[line >> 3] & (1 << (line & 7)))
                {
                    for (int i = 0; i < DS_LCD_WIDTH / 2; ++i)
                    {
                        dst[0] = src[1];
                        dst[DS_LCD_WIDTH * DS_LCD_HEIGHT] = src[0];
                        dst++;
                        src += 2;
                    }
                }
                else
                {
                    memcpy(dst, dst - 256, 256);
                    memcpy(dst + 256 * 192, dst + 256 * 191, 256);
                    dst += 128;
                }
            }
        }
    }
}

HRESULT DSCapture::openDevice()
{
    HRESULT hr = S_OK;

    handlesOpen = false;
    hr = retrieveDevicePath(devicePath, sizeof(devicePath));

    if (FAILED(hr))
    {
        return hr;
    }

    deviceHandle = CreateFile(devicePath,
                              GENERIC_WRITE | GENERIC_READ,
                              FILE_SHARE_WRITE | FILE_SHARE_READ,
                              NULL,
                              OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                              NULL);

    if (deviceHandle == INVALID_HANDLE_VALUE)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        return hr;
    }

    bool initialized = WinUsb_Initialize(deviceHandle, &winusbHandle);
    if (initialized == false)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        CloseHandle(deviceHandle);
        return hr;
    }

    handlesOpen = true;
    return hr;
}

void DSCapture::closeDevice()
{
    if (!handlesOpen)
    {
        return;
    }

    WinUsb_Free(winusbHandle);
    CloseHandle(deviceHandle);
    handlesOpen = false;
}

HRESULT DSCapture::retrieveDevicePath(char* path, ULONG bufLen)
{
    CONFIGRET cr = CR_SUCCESS;
    HRESULT hr = S_OK;
    PTSTR deviceInterfaceList = NULL;
    ULONG deviceInterfaceListLength = 0;

    do
    {
        cr = CM_Get_Device_Interface_List_Size(&deviceInterfaceListLength,
                                               (LPGUID) &GUID_DSCapture,
                                               NULL,
                                               CM_GET_DEVICE_INTERFACE_LIST_PRESENT);

        if (cr != CR_SUCCESS)
        {
            hr = HRESULT_FROM_WIN32(CM_MapCrToWin32Err(cr, ERROR_INVALID_DATA));
            break;
        }

        deviceInterfaceList = (PTSTR)HeapAlloc(GetProcessHeap(),
                                               HEAP_ZERO_MEMORY,
                                               deviceInterfaceListLength * sizeof(TCHAR));

        if (deviceInterfaceList == NULL)
        {
            hr = E_OUTOFMEMORY;
            break;
        }

        cr = CM_Get_Device_Interface_List((LPGUID) &GUID_DSCapture,
                                          NULL,
                                          deviceInterfaceList,
                                          deviceInterfaceListLength,
                                          CM_GET_DEVICE_INTERFACE_LIST_PRESENT);

        if (cr != CR_SUCCESS)
        {
            HeapFree(GetProcessHeap(), 0, deviceInterfaceList);

            if (cr != CR_BUFFER_SMALL)
            {
                hr = HRESULT_FROM_WIN32(CM_MapCrToWin32Err(cr, ERROR_INVALID_DATA));
            }
        }
    } while (cr == CR_BUFFER_SMALL);

    if (FAILED(hr))
    {
        return hr;
    }

    // If list is empty, no devices were found
    if (*deviceInterfaceList == TEXT('\0'))
    {
        hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
        HeapFree(GetProcessHeap(), 0, deviceInterfaceList);
        return hr;
    }

    hr = StringCbCopy(path, bufLen, deviceInterfaceList);
    HeapFree(GetProcessHeap(), 0, deviceInterfaceList);
    return hr;
}

bool DSCapture::queryDeviceEndpoints()
{
    if (winusbHandle == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    bool result = true;

    USB_INTERFACE_DESCRIPTOR interfaceDescriptor;
    ZeroMemory(&interfaceDescriptor, sizeof(USB_INTERFACE_DESCRIPTOR));
    WINUSB_PIPE_INFORMATION pipe;
    ZeroMemory(&pipe, sizeof(WINUSB_PIPE_INFORMATION));

    result = WinUsb_QueryInterfaceSettings(winusbHandle, 0, &interfaceDescriptor);
    if (result)
    {
        for (int i = 0; i < interfaceDescriptor.bNumEndpoints; i++)
        {
            result = WinUsb_QueryPipe(winusbHandle, 0, i, &pipe);
            if (result)
            {
                if (pipe.PipeType == UsbdPipeTypeBulk)
                {
                    if (USB_ENDPOINT_DIRECTION_IN(pipe.PipeId))
                    {
                        printf("Endpoint index: %d - Pipe type: Bulk - Pipe ID: 0x%x - Maximum packet size: %d\n", i, pipe.PipeId, pipe.MaximumPacketSize);
                        bulkPipeInId = pipe.PipeId;
                        maxPacketSize = pipe.MaximumPacketSize;
                    }
                }
            }
        }
    }

    return result;
}

bool DSCapture::sendToDefaultEndpoint(uint8_t request, uint16_t value, uint16_t length, uint8_t *buf)
{
    if (winusbHandle == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    bool result = true;

    UsbDeviceRequest usbRequest;
    usbRequest.bits.recipient = 0;
    usbRequest.bits.type = 2;
    usbRequest.bits.dataDirection = 0;

    WINUSB_SETUP_PACKET setupPacket;
    ZeroMemory(&setupPacket, sizeof(WINUSB_SETUP_PACKET));
    setupPacket.RequestType = usbRequest.requestType;
    setupPacket.Request = request;
    setupPacket.Value = value;
    setupPacket.Index = 0;
    setupPacket.Length = length;

    unsigned long sent;
    result = WinUsb_ControlTransfer(winusbHandle, setupPacket, buf, length, &sent, 0);
    return result;
}

bool DSCapture::recvFromDefaultEndpoint(uint8_t request, uint16_t length, uint8_t* buf)
{
    if (winusbHandle == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    bool result = true;

    UsbDeviceRequest usbRequest;
    usbRequest.bits.recipient = 0;
    usbRequest.bits.type = 2;
    usbRequest.bits.dataDirection = 1;

    WINUSB_SETUP_PACKET setupPacket;
    ZeroMemory(&setupPacket, sizeof(WINUSB_SETUP_PACKET));
    setupPacket.RequestType = usbRequest.requestType;
    setupPacket.Request = request;
    setupPacket.Value = 0;
    setupPacket.Index = 0;
    setupPacket.Length = length;

    unsigned long sent;
    result = WinUsb_ControlTransfer(winusbHandle, setupPacket, buf, length, &sent, NULL);
    return result;
}

bool DSCapture::readFromBulk(uint8_t* buf, int length, PULONG transferred)
{
    if (winusbHandle == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    
    bool result = true;
    result = WinUsb_ReadPipe(winusbHandle, bulkPipeInId, buf, length, transferred, NULL);
    return result;
}

bool DSCapture::getMaximumTransferSize(unsigned long* mts)
{
    unsigned long length = sizeof(unsigned long);

    bool result = WinUsb_GetPipePolicy(winusbHandle, bulkPipeInId, 0x08, &length, mts);
    if (!result)
    {
        printf("Couldn't get maximum transfer size: %d\n", GetLastError());
    }

    return result;
}
