#pragma once
#include <ntifs.h>
#include <ntddk.h>
#include <intrin.h>

#include "ia32.h"

#pragma warning( disable : 4201)

#define kprintf(...) DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, __VA_ARGS__)
//#define kprintf(...) 

typedef INT64 uint64_t;

typedef union _VIRTUAL_ADDRESS
{
    PVOID value;
    struct
    {
        uint64_t offset : 12;
        uint64_t pt_index : 9;
        uint64_t pd_index : 9;
        uint64_t pdp_index : 9;
        uint64_t pml4_index : 9;
        uint64_t reserved : 16;
    };
} VIRTUAL_ADDRESS, * PVIRTUAL_ADDRESS;

//
// arch: x64
// page map level 4 entry definition
//
typedef union _PML4E
{
    uint64_t value;
    struct
    {
        uint64_t present : 1;
        uint64_t writable : 1;
        uint64_t user_access : 1;
        uint64_t write_through : 1;
        uint64_t cache_disabled : 1;
        uint64_t accessed : 1;
        uint64_t ignored_3 : 1;
        uint64_t size : 1;
        uint64_t ignored_2 : 4;
        uint64_t pfn : 36;
        uint64_t reserved_1 : 4;
        uint64_t ignored_1 : 11;
        uint64_t execution_disabled : 1;
    };
} PML4E, * PPML4E;

//
// arch: x64
// page directory pointer entry definition
//
typedef union PDPE
{
    uint64_t value;
    struct
    {
        uint64_t present : 1;
        uint64_t writable : 1;
        uint64_t user_access : 1;
        uint64_t write_through : 1;
        uint64_t cache_disabled : 1;
        uint64_t accessed : 1;
        uint64_t ignored_3 : 1;
        uint64_t size : 1;
        uint64_t ignored_2 : 4;
        uint64_t pfn : 36;
        uint64_t reserved_1 : 4;
        uint64_t ignored_1 : 11;
        uint64_t execution_disabled : 1;
    };
} PDPE, * PPDPE;

//
// arch: x64
// page directory entry definition
//
typedef union _PDE
{
    uint64_t value;
    struct
    {
        uint64_t present : 1;
        uint64_t writable : 1;
        uint64_t user_access : 1;
        uint64_t write_through : 1;
        uint64_t cache_disabled : 1;
        uint64_t accessed : 1;
        uint64_t ignored1 : 1;
        uint64_t size : 1;
        uint64_t ignored_2 : 4;
        uint64_t pfn : 36;
        uint64_t reserved_1 : 4;
        uint64_t ignored_1 : 11;
        uint64_t execution_disabled : 1;
    };
} PDE, * PPDE;

//
// arch: x64
// page table entry definition
//
typedef union _PTE
{
    uint64_t value;
    struct
    {
        uint64_t present : 1;
        uint64_t writable : 1;
        uint64_t user_access : 1;
        uint64_t write_through : 1;
        uint64_t cache_disabled : 1;
        uint64_t accessed : 1;
        uint64_t dirty : 1;
        uint64_t access_type : 1;
        uint64_t global : 1;
        uint64_t ignored_2 : 3;
        uint64_t pfn : 36;
        uint64_t reserved_1 : 4;
        uint64_t ignored_3 : 7;
        uint64_t protection_key : 4;
        uint64_t execution_disabled : 1;
    };
} PTE, * PPTE;

NTSTATUS    ReadPhysicalAddress(uint64_t TargetAddress, PVOID lpBuffer, SIZE_T Size, SIZE_T* BytesRead);
PPTE        PTEForVirtualAddress(uint64_t dtb, uint64_t virtaddr);