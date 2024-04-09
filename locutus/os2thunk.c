#include <windows.h>
#include <stdio.h>
#include "os2thunk.h"
#include "os2mem.h"

uint32_t DosFlatToSel(OS2PTR32 ptr){
	uint16_t selector = (ptr >> 3) >> 16;
	uint16_t offset = ptr & 0xFFFF;
	
	return (selector << 16) | offset;
}  

OS2PTR32 DosSelToFlat(uint32_t farptr){
	uint16_t selector = farptr >> 16;
	uint16_t offset = farptr & 0xFFFF;
	
	return ((((selector & 0x1FFF) >> 3) | 0x7) << 16) | offset;
}

USHORT DosPutMessage_Impl(OS2HFILE hFile, USHORT msgLen, PCHAR pStr){
	return write(TranslateHFILE(hFile), pStr, msgLen);
}

USHORT Dos32PutMessage(OS2HFILE hFile, USHORT msgLen, OS2PTR32 pStr){
	return DosPutMessage_Impl(hFile, msgLen, TranslateEmulatedToVirtualAddress(pStr));
}

PVOID MsgImportThunks[] = {6, //number of entries in table
	0,
	0,
	0,
	0,
	0,
	Dos32PutMessage
};

extern PVOID DosImportThunks[];

PVOID ModuleThunkTable[256] = {1, //number of entries in table
	0, //Module 0
	0
};

uint32_t HostCall(uint32_t DestinationAddress, uint32_t* pParamList, uint32_t dwParamCount){
	DWORD old_esp;

	__asm {
		mov old_esp, esp

		mov ecx, dwParamCount
		mov edx, pParamList
		lea eax, [edx + ecx * 4 - 4]

		loop_start:
			cmp eax, edx
			jb call_function
			push dword ptr[eax]
			sub eax, 4
			jmp loop_start

		call_function :
			mov eax, DestinationAddress
			call eax

		mov esp, old_esp
}
	};

OS2PTR32 GenerateThunk(PVOID ThunkPtr){
	OS2PTR32 pEmuThunk = PFToAddr(VMMAllocPages(0x800, 1));
	PBYTE pThunkBits = TranslateEmulatedToVirtualAddress(pEmuThunk);
	
	*(pThunkBits) = 0xb8;
	*(uint32_t*)(pThunkBits+1) = ThunkPtr;
	*(pThunkBits+5) = 0xcd;
	*(pThunkBits+6) = 0x80;
	*(pThunkBits+7) = 0xc3;
	
	return pEmuThunk;
}

OS2PTR32 GetModuleProcAddrByOrdinal(WORD ModuleId, DWORD ImportId){
	uint32_t* ImportThunkTable;
	uint32_t ImportAddr;
	
	printf("TRACE: GetModuleProcAddrByOrdinal(%x, %x)\n", ModuleId, ImportId);
	
	if(ModuleId <= ModuleThunkTable[0]){
		ImportThunkTable = ModuleThunkTable[ModuleId + 1];
		
		if(ImportId <= ImportThunkTable[0]){
			ImportAddr = ImportThunkTable[ImportId + 1];
			
			if(ImportAddr & 0x80000000){ //high bit set, therefore we've already mapped the thunk into the OS/2 address space
				return ImportAddr & 0x7FFFFFFF;
			}else{ //this is the address of the hostside thunk, so generate it
			
				if(ImportAddr == 0){
					fprintf(stderr, "Attempting to access invalid import %d from module %d\n", ImportId, ModuleId);
					return 0;
				}else{
					ImportThunkTable[ImportId + 1] = GenerateThunk(ImportAddr) | 0x80000000;
					fprintf(stdout, "Mapping import %d from module %d to %p (pass-through to %p)\n", ImportId, ModuleId, ImportThunkTable[ImportId + 1] & 0x7FFFFFFF, ImportAddr);
					return ImportThunkTable[ImportId + 1] & 0x7FFFFFFF;
				}
			}
			
		}else{
			fprintf(stderr, "Attempting to access invalid import %d from module %d\n", ImportId, ModuleId);
			return 0;
		}
		
	}else{
		fprintf(stderr, "Attempting to access invalid library %d\n", ModuleId);
		return 0;
	}
}