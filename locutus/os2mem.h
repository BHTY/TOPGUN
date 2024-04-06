#pragma once

#include "../maverick/run386.h"

typedef uint32_t pfn_t;
typedef uint32_t OS2PTR32;

extern char* Os2Base;

#define AddrToPF(addr)	(addr >> 12)
#define PFToAddr(pf)	(pf << 12)

uint8_t* TranslateEmulatedToVirtualAddress(uint32_t addr) ;
uint32_t TranslateVirtualToEmulatedAddress(uint8_t* addr);

void VMMInit();

pfn_t VMMScanAddressSpace(pfn_t vbase, uint32_t nPages);
void VMMReserveRegion(pfn_t vbase, uint32_t nPages);
void VMMReleaseRegion(pfn_t vbase, uint32_t nPages);
void VMMFreePages(pfn_t vbase, uint32_t nPages);
pfn_t VMMAllocPages(pfn_t vbase, uint32_t nPages);