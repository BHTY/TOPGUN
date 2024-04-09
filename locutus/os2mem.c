#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../maverick/run386.h"
#include <windows.h>
#include "../common/cmd.h"
#include <setjmp.h>
#include "os2mem.h"

char* Os2Base;
uint32_t PFBitmap[4096];

#define PFDword(pfn)	(PFBitmap[(pfn)/32])
#define PFStatus(pfn)	((   PFDword((pfn)) >> ((pfn) % 32)) & 0x1)

#define MarkPageA(pfn)		PFDword((pfn)) |= (1 << ((pfn) % 32))
#define MarkPageF(pfn)		PFDword((pfn)) &= ~(1 << ((pfn) % 32))

/*
OS/2 Virtual Memory Management

When the emulator process is started, a 512MB block representing the entire OS/2 user address space is reserved. All emulated addresses are
considered to be relative to this base. 

When the OS/2 process needs to allocate a page of memory (for example, when loading an executable), it will call a function passing in the
desired base address and number of pages needed, with the achieved base address being returned. The internal behavior is that it searches for
a free block of sufficient size in the page allocation bitmap starting at the desired base address, and returns the first block found. On the
host side, the address range is committed.

*/

uint8_t* TranslateEmulatedToVirtualAddress(uint32_t addr) {
	
	return Os2Base + addr;
}

uint32_t TranslateVirtualToEmulatedAddress(uint8_t* addr){
	return addr - Os2Base;
}

uint32_t io_read_32(uint16_t port) {
}

uint32_t io_read_16(uint16_t port) {
}

uint32_t io_read_8(uint16_t port) {
}

uint32_t bus_read_8(uint32_t addr) {
	//printf("Reading from address %05x\n", addr);
	return (uint32_t) * (TranslateEmulatedToVirtualAddress(addr));
}

uint32_t bus_read_16(uint32_t addr) {
	return (uint32_t) * (uint16_t*)(TranslateEmulatedToVirtualAddress(addr));
}

uint32_t bus_read_32(uint32_t addr) {
	return *(uint32_t*)(TranslateEmulatedToVirtualAddress(addr));
}

void bus_write_8(uint32_t addr, uint8_t value) {
	//printf("Writing %02x to %05x\n", value, addr);
	*(TranslateEmulatedToVirtualAddress(addr)) = value;
}

void bus_write_16(uint32_t addr, uint16_t value) {
	*(uint16_t*)(TranslateEmulatedToVirtualAddress(addr)) = value;
}

void bus_write_32(uint32_t addr, uint32_t value) {
	*(uint32_t*)(TranslateEmulatedToVirtualAddress(addr)) = value;
}




void VMMInit(){
	Os2Base = VirtualAlloc(NULL, 512 * 1024 * 1024, MEM_RESERVE, PAGE_READWRITE);
	memset(PFBitmap, 0, sizeof(PFBitmap));
	PFBitmap[0] = 0xFFFF; //reserve first 64kb
}

pfn_t FindFirstFreePage(pfn_t index){ //find first free page starting at index
	pfn_t cur_index = index;
	pfn_t i;
	
	if(PFDword(cur_index) == -1){ //no free pages found at the current index dword
		cur_index += (32-(cur_index) % 32);
	}else{
		for(; cur_index < (32-(index) % 32); cur_index++){
			if(!(PFStatus(cur_index))) return cur_index;
		}
	}
	
	//now we're allocated to dword boundaries
	while(cur_index < (4096*4096*32)){
		if(PFDword(cur_index) == -1){
			cur_index += 32;
		}else{
			for(i = cur_index; i < cur_index + 32; i++){
				if(!(PFStatus(i))) return i;
			}
		}
	}
	
	return NULL;
}

uint32_t NumberOfPagesFreeHere(pfn_t index){ //return number of contiguous virtual address space free starting at index
	pfn_t i = index;
	int n = 0;
	
	while(!(PFStatus(i++))){
		n++;
	}
	
	//printf("Index = %p Page frame = %p PFStatus(i)=%d n=%d\n", index, i, PFStatus(i), n);
	
	return i - index;
}

pfn_t VMMScanAddressSpace(pfn_t vbase, uint32_t nPages){
	pfn_t current_pf = vbase;
	pfn_t temp;
	
	while(current_pf < (4096 * 4096 * 32)){
		temp = FindFirstFreePage(current_pf);
		
		//printf("Found %d free pages @ %p (%p), needs %d\n", NumberOfPagesFreeHere(temp), temp, PFDword(temp), nPages);
		
		if(NumberOfPagesFreeHere(temp) >= nPages){
			return temp;
		}
		
		current_pf = temp + NumberOfPagesFreeHere(temp);
	}
	
	return NULL;
}

void VMMReserveRegion(pfn_t vbase, uint32_t nPages){ //mark pages as reserved in bitmap and commit on host
	int i;

	VirtualAlloc(TranslateEmulatedToVirtualAddress(PFToAddr(vbase)), nPages * 4096, MEM_COMMIT, PAGE_READWRITE);
	
	for(i = 0; i < nPages; i++){
		MarkPageA(vbase + i);
	}
}

void VMMReleaseRegion(pfn_t vbase, uint32_t nPages){
	
}

void VMMFreePages(pfn_t vbase, uint32_t nPages){
	VMMReleaseRegion(vbase, nPages);
}

pfn_t VMMAllocPages(pfn_t vbase, uint32_t nPages){ //returns page frame index of base page
	pfn_t ActualBase = VMMScanAddressSpace(vbase, nPages);
	
	if(ActualBase == NULL) return NULL;
	
	VMMReserveRegion(ActualBase, nPages);
	
	return ActualBase;
}