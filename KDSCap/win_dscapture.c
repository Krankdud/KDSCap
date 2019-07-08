#include <Windows.h>
#include <strsafe.h>
#include <winusb.h>
#include <winnt.h>
#include <cfgmgr32.h>
#include "win_dscapture.h"

typedef struct _DeviceData
{
    bool handlesOpen;
    WINUSB_INTERFACE_HANDLE winusbHandle;
    HANDLE deviceHandle;
    char devicePath[260];
} DeviceData;

static DeviceData deviceData;

HRESULT openDevice();
void closeDevice();
HRESULT retrieveDevicePath(char* path, ULONG bufLen);

bool win_dscapture_init()
{
    USB_DEVICE_DESCRIPTOR deviceDesc;
    ULONG lengthReceived;

    HRESULT hr = openDevice();
    if (FAILED(hr))
    {
        printf("Failed looking for device, HRESULT 0x%x\n", hr);
        return false;
    }

    bool result = WinUsb_GetDescriptor(deviceData.winusbHandle,
                                       USB_DEVICE_DESCRIPTOR_TYPE,
                                       0,
                                       0,
                                       (PBYTE) &deviceDesc,
                                       sizeof(deviceDesc),
                                       &lengthReceived);
    if (result == false || lengthReceived != sizeof(deviceDesc))
    {
        printf("Error among LastError %d or lengthReceived %d\n", result == false ? GetLastError() : 0, lengthReceived);
        closeDevice();
        return false;
    }

    printf("Device found: VID_%04X&PID_%04X", deviceDesc.idVendor, deviceDesc.idProduct);
    return true;
}

void win_dscapture_deinit()
{
    closeDevice();
}

HRESULT openDevice()
{
    HRESULT hr = S_OK;

    deviceData.handlesOpen = false;
    hr = retrieveDevicePath(deviceData.devicePath, sizeof(deviceData.devicePath));

    if (FAILED(hr))
    {
        return hr;
    }

    deviceData.deviceHandle = CreateFile(deviceData.devicePath,
                                         GENERIC_WRITE | GENERIC_READ,
                                         FILE_SHARE_WRITE | FILE_SHARE_READ,
                                         NULL,
                                         OPEN_EXISTING,
                                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                                         NULL);

    if (deviceData.deviceHandle == INVALID_HANDLE_VALUE)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        return hr;
    }

    bool initialized = WinUsb_Initialize(deviceData.deviceHandle, &deviceData.winusbHandle);
    if (initialized == false)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        CloseHandle(deviceData.deviceHandle);
        return hr;
    }

    deviceData.handlesOpen = true;
    return hr;
}

void closeDevice()
{
    if (!deviceData.handlesOpen)
    {
        return;
    }

    WinUsb_Free(deviceData.winusbHandle);
    CloseHandle(deviceData.deviceHandle);
    deviceData.handlesOpen = false;
}

HRESULT retrieveDevicePath(char* path, ULONG bufLen)
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
