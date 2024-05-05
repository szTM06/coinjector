#include <iostream>
#include <string>
#include <Windows.h>

#define CTL_ADDCURRENTTHREAD            CTL_CODE( FILE_DEVICE_UNKNOWN, 0x20, METHOD_BUFFERED, FILE_ANY_ACCESS )
#define CTL_DELCURRENTTHREAD            CTL_CODE( FILE_DEVICE_UNKNOWN, 0x10, METHOD_BUFFERED, FILE_ANY_ACCESS )

#define CTL_ADDBYIDTHREAD               CTL_CODE( FILE_DEVICE_UNKNOWN, 0x22, METHOD_BUFFERED, FILE_ANY_ACCESS )
#define CTL_DELBYIDTHREAD               CTL_CODE( FILE_DEVICE_UNKNOWN, 0x12, METHOD_BUFFERED, FILE_ANY_ACCESS )

#define CTL_ENABLEGLOBALHOOK            CTL_CODE( FILE_DEVICE_UNKNOWN, 0x40, METHOD_BUFFERED, FILE_ANY_ACCESS )
#define CTL_DISABLEGLOBALHOOK           CTL_CODE( FILE_DEVICE_UNKNOWN, 0x80, METHOD_BUFFERED, FILE_ANY_ACCESS )

#define DRV_STANDARD_HANDLE             L"\\\\.\\Buh"

int main(int argc, char** argv) {
    int option = 0;
    int thread = 0;

    if (argc == 2) {
        if (strcmp(argv[1], "-enable") == 0) {
            option = 1;
        }
        else {
            option = 2;
        }
    }
    else if (argc == 3) {
        thread = strtol(argv[2], 0, 10);
        if (strcmp(argv[1], "-add") == 0) {
            option = 3;
        }
        else {
            option = 4;
        }
    }
    else {
        std::cout << "Error. Usage " << argv[0] << " (-enable/-disable) | (-add <threadid>/-del <threadid>)" << std::endl;
        return -1;
    }

    auto DrvHandle = CreateFileW(DRV_STANDARD_HANDLE, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    bool status = 0;

    if (DrvHandle == INVALID_HANDLE_VALUE) {
        std::cout << "err" << std::endl;
        std::cout << GetLastError();
        return 2;
    }

    if (option == 1) {
        return DeviceIoControl(DrvHandle, CTL_ENABLEGLOBALHOOK, 0, 0, 0, 0, 0, (LPOVERLAPPED)NULL);
    }
    
    if (option == 2) {
        return DeviceIoControl(DrvHandle, CTL_DISABLEGLOBALHOOK, 0, 0, 0, 0, 0, (LPOVERLAPPED)NULL);
    }

    if (option == 3) {
        HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, 0, thread);
        if (hThread == 0) {
            return -1;
        }
        SuspendThread(hThread);
        INT64 threadBuf = thread;
        if (DeviceIoControl(DrvHandle, CTL_ADDBYIDTHREAD, &threadBuf, sizeof(INT64), 0, 0, 0, (LPOVERLAPPED)NULL) != 0) {
            std::cout << "Error communicating with driver - Code " << GetLastError() << std::endl;
            return -1;
        }
        ResumeThread(hThread);
        return 0;
    }

    if (option == 4) {
        HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, 0, thread);
        if (hThread == 0) {
            return -1;
        }
        SuspendThread(hThread);
        INT64 threadBuf = thread;
        if (DeviceIoControl(DrvHandle, CTL_DELBYIDTHREAD, &threadBuf, sizeof(INT64), 0, 0, 0, (LPOVERLAPPED)NULL) != 0) {
            std::cout << "Error communicating with driver - Code " << GetLastError() << std::endl;
            return -1;
        }
        ResumeThread(hThread);
        return 0;
    }
}
