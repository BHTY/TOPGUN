#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "lx.h"

#include "os2mem.h"
#include "os2thunk.h"

#define ESCAPE_ADDRESS 0x0000BEEF

i386 cpu;
i386* Pcpu;

void init_386(i386* pCPU){
	memset(pCPU, 0, sizeof(i386));

	pCPU->sgmt_override = DS;
	pCPU->running = 1;

	//mark everything as writable valid data USE16 nonsystem segments with limit 4GB
	pCPU->es.access = 0x80 | 0x10 | 0x2;
	pCPU->fs.access = 0x80 | 0x10 | 0x2;
	pCPU->gs.access = 0x80 | 0x10 | 0x2;
	pCPU->ds.access = 0x80 | 0x10 | 0x2;
	pCPU->ss.access = 0x80 | 0x10 | 0x2;
	pCPU->cs.access = 0x80 | 0x10 | 0x8 | 0x2;

	pCPU->es.base = 0;
	pCPU->fs.base = 0;
	pCPU->gs.base = 0;
	pCPU->ds.base = 0;
	pCPU->ss.base = 0;
	pCPU->cs.base = 0;

	pCPU->cs.flags = 0x4;
	pCPU->ss.flags = 0x4;
}

OS2PTR32 Os2Mmap(OS2PTR32 LpBaseAddress, DWORD dwSize){
	return PFToAddr(VMMAllocPages(AddrToPF(LpBaseAddress), dwSize / 4096));
}

PBYTE WinMmap(PVOID LpBaseAddress, DWORD dwSize, DWORD AccessPermissionFlags){
	LPVOID Result;
	
	Result = VirtualAlloc(LpBaseAddress, dwSize, MEM_COMMIT | MEM_RESERVE, AccessPermissionFlags);
	
	if(Result == NULL){
		Result = VirtualAlloc(NULL, dwSize, MEM_COMMIT | MEM_RESERVE, AccessPermissionFlags);
	}
	
	return Result;
}

INT CheckFormat(PBYTE* ppPointer, DWORD* pLength){
	DWORD HeaderOffset;
	LxHeader* pLX;

	if(*pLength < 196){	 
		fprintf(stderr, "Invalid format\n");
		return 0;
	}

	HeaderOffset = *((DWORD*)(*ppPointer + 0x3C));

	if((HeaderOffset + sizeof(LxHeader)) >= *pLength){
		fprintf(stderr, "Invalid format\n");
		return 0;
	}	

	//skip DOS 2.0 MZ stub
	*ppPointer += HeaderOffset;
	*pLength -= HeaderOffset;

	pLX = *ppPointer;

	//do some sanity checking

	return 1;
}

void doFixup(OS2PTR32 pDst, uint16_t Offset, uint32_t FinalVal, uint16_t FinalVal2, uint32_t FinalSize){
	uint8_t* page = TranslateEmulatedToVirtualAddress(pDst);
	
	switch(FinalSize){
		case 1: *(uint8_t*)(page + Offset) = FinalVal; break;
		case 2: *(uint16_t*)(page + Offset) = FinalVal; break;
		case 4: *(uint32_t*)(page + Offset) = FinalVal; break;
		case 6:
			*(uint16_t*)(page+Offset) = FinalVal2;
			*(uint32_t*)(page+Offset+2) = FinalVal;
			break;
		default: break;
	}
}

VOID FixupPage(LxHeader* pLX, LxObjectTableEntry* pObj, OS2PTR32 pDst){
	PBYTE ExeBuffer = pLX;
	DWORD *FixupPage = (DWORD*)(ExeBuffer + pLX->fixup_page_table_offset) + (pObj->page_table_index - 1);
	DWORD FixupOffset = *FixupPage;
	DWORD FixupLen = FixupPage[1] - FixupPage[0];
	PBYTE pFixup = (ExeBuffer + pLX->fixup_record_table_offset) + FixupOffset;
	PBYTE pFixupEnd = pFixup + FixupLen;
	
	while(pFixup < pFixupEnd){
		BYTE SrcType = *(pFixup++);
		BYTE FixupFlags = *(pFixup++);
		BYTE SrclistCount = 0;
		WORD SrcOffset = 0;
		DWORD FinalVal = 0;
		WORD FinalVal2 = 0;
		DWORD FinalSize = 0;
		DWORD Additive;
		
		printf("\t\t\tSource Type: %x\n", SrcType & 0xF);
		
		//determine size
		switch(SrcType & 0xF){
			case 0x0: //byte fixup
				FinalSize = 1;
				break;
			case 0x2: //16-bit selector fixup
			case 0x5: //16-bit offset fixup
				FinalSize = 2;
				break;
			case 0x3: //16:16 pointer fixup
			case 0x7: //32-bit offset fixup
			case 0x8: //32-bit self-relative offset fixup
				FinalSize = 4;
				break;
			case 0x6: //16:32 pointer fixup
				FinalSize = 6;
				break;
		}
		
		if(SrcType & 0x20){ //contains source list
			SrclistCount = *(pFixup++);
		}else{
			SrcOffset = *(WORD*)pFixup;
			pFixup += 2;
		}
		
		switch(FixupFlags & 0x3){
			case 0x00: { //Internal fixup record
				fprintf(stderr, "FIXME INTERNAL FIXUP!\n");
				exit(1);
				break;
			}
			
			case 0x01: { //Import by ordinal
				WORD ModuleId = 0;
				DWORD ImportId = 0;
				
				if(FixupFlags & 0x40){ //16-bit module ID
					ModuleId = *(WORD*)pFixup;
					pFixup += 2;
				}else{ //8-bit module ID
					ModuleId = *(pFixup++);
				}
				
				if(FixupFlags & 0x80){ //8-bit ordinal
					ImportId = *(pFixup++);
				}else if(FixupFlags & 0x10){ //32-bit ordinal
					ImportId = *(DWORD*)pFixup;
					pFixup += 4;
				}else{ //16-bit ordinal
					ImportId = *(WORD*)pFixup;
					pFixup += 2;
				}

				FinalVal = GetModuleProcAddrByOrdinal(ModuleId, ImportId);
				
				break;
			}
			
			case 0x02: { //Import by name 
				fprintf(stderr, "FIXME IMPORT BY NAME!\n");
				exit(1);
				break;
			}
			
			case 0x03: { //Internal entry table
				fprintf(stderr, "FIXME INTERNAL ENTRY!\n");
				exit(1);
				break;
			}
		}
		
		Additive = 0;
		
		if(FixupFlags & 0x4){ //has additive
			if(FixupFlags & 0x20){ //32-bit value
				Additive = *(DWORD*)pFixup;
				pFixup += 4;
			}else{ //16-bit
				Additive = *(WORD*)pFixup;
				pFixup += 2;
			}
		}
		FinalVal += Additive;
		
		if(SrcType & 0x20){ //source list
			int i;
			
			for(i = 0; i < SrclistCount; i++){
				uint16_t Offset = *(uint16_t*)pFixup; pFixup += 2;
				if((SrcType & 0xF) == 0x08){ //self-relative?
					doFixup(pDst, Offset, FinalVal - (pDst+Offset+4), 0, FinalSize);
				}else{
					doFixup(pDst, Offset, FinalVal, FinalVal2, FinalSize);		
				}
			}
		}else{
			if((SrcType & 0xF) == 0x08){ //self-relative?
				doFixup(pDst, SrcOffset, FinalVal - (pDst+SrcOffset+4), 0, FinalSize);
			}else{
				doFixup(pDst, SrcOffset, FinalVal, FinalVal2, FinalSize);		
			}
		}
	}
}

//https://github.com/icculus/2ine/blob/161cde892edc68e07964830775b7ed7642ff5e91/lx_loader.c#L415

uint32_t Exec32(i386* pCPU, uint32_t dwTargetAddress, uint32_t* dwParamList, uint32_t nParams) {
	uint32_t dwReturnAddress = pCPU->eip;
	int i;
	uint32_t old_eip, older_eip;

	for (i = nParams - 1; i >= 0; i--) {
		i386_push(pCPU, dwParamList[i]);
	}

	i386_push(pCPU, ESCAPE_ADDRESS);

	pCPU->eip = dwTargetAddress;
	
	printf("Commencing execution with ESP=%p\n", pCPU->esp);	

	while (pCPU->eip != ESCAPE_ADDRESS) {
		old_eip = pCPU->eip;
		older_eip = pCPU->eip;
		
		printf("%p: ", pCPU->eip);
		
		dis386(TranslateEmulatedToVirtualAddress(older_eip), older_eip, 1, 1, 0, 0);
		
		i386_step(pCPU);
		
		/*for (; old_eip < pCPU->old_eip; old_eip++) {
			printf("%02x ", bus_read_8(pCPU->cs.base + old_eip));
		}*/
		
		//dis386(TranslateEmulatedToVirtualAddress(older_eip), older_eip, 1, 1, 0, 0);

		//printf("\n");
	}
	
	dump_386(pCPU);
	 
	return pCPU->eax;
}

uint32_t LaunchOS2(uint32_t entrypoint, uint32_t sp, char* pEnv, char* pCmd, uint32_t hmod){
	uint32_t dwParamList[] = {hmod, 0, pEnv, pCmd}; //modhandle, 0, env, cmd
	
	cpu.esp = sp;
	
	return Exec32(&cpu, entrypoint, dwParamList, 4);
}

void ResolveImports(PBYTE Buffer, LxHeader* pLX){
	PBYTE pImpTbl = Buffer + pLX->import_module_table_offset;
	DWORD dwTblSz = pLX->import_proc_table_offset - pLX->import_module_table_offset;
	char TempBuffer[128];
	
	while(pImpTbl < (Buffer + pLX->import_proc_table_offset)){
		memcpy(TempBuffer, pImpTbl + 1, *pImpTbl);
		TempBuffer[*pImpTbl] = 0;
		printf("Loading library %s: ", TempBuffer);
		
		if(strcmp(TempBuffer, "MSG") == 0){
			printf("SUCCEEDED\n");
			ModuleThunkTable[*(uint32_t*)(ModuleThunkTable) + 1] = MsgImportThunks;
		}else if(strcmp(TempBuffer, "DOSCALLS") == 0){
			printf("SUCCEEDED\n");
			ModuleThunkTable[*(uint32_t*)(ModuleThunkTable) + 1] = DosImportThunks;
		}else{
			printf("FAILED\n");
		}
		
		pImpTbl += (*pImpTbl + 1);
		(*(uint32_t*)(ModuleThunkTable))++;
	}
}

DWORD LoadLX(LPSTR FileName, PBYTE Buffer, DWORD Length){
	DWORD var_eip, var_esp;
	PSTR pCmdLine, pEnvironment;
	
	INT i;
	PBYTE OriginalBuffer = Buffer;
	LxHeader* pLX;
	LxObjectTableEntry* pObj;

	if(!CheckFormat(&Buffer, &Length)){
		exit(1);
	}

	pLX = Buffer;
	pObj = Buffer + pLX->object_table_offset;

	printf("%d objects | %d pages\n", pLX->module_num_objects, pLX->module_num_pages);
	printf("CPU type: %d | OS version: %d\n", pLX->cpu_type, pLX->os_type);
	printf("EIP Object Number: %d | ESP Object Number: %d\n", pLX->eip_object, pLX->esp_object);
	
	ResolveImports(Buffer, pLX);

	for(i = 0; i < pLX->module_num_objects; i++, pObj++){
		DWORD ObjectVirtualSize = pObj->virtual_size;
		PBYTE ObjectBaseAddress = pObj->reloc_base_addr;
		LxObjectPageTableEntry *pObjPage;
		INT PageNumber;
		DWORD NumberPageTableEntries;
		OS2PTR32 pDst;
		OS2PTR32 pObject;
		DWORD RemainingBytes;

		if((ObjectVirtualSize % pLX->page_size) != 0){
			ObjectVirtualSize += pLX->page_size - (ObjectVirtualSize % pLX->page_size);
		}

		printf("Object %d: Address 0x%p (%d bytes)\n", i, ObjectBaseAddress, ObjectVirtualSize);
		
		pObject = Os2Mmap(ObjectBaseAddress, ObjectVirtualSize);//WinMmap(ObjectBaseAddress, ObjectVirtualSize, PAGE_EXECUTE_READWRITE);
		//pObject = WinMmap(ObjectBaseAddress, ObjectVirtualSize, PAGE_EXECUTE_READWRITE);
		
		printf("\tMapped to 0x%p\n", pObject);

		pObjPage = Buffer + pLX->object_page_table_offset;
		pObjPage += pObj->page_table_index - 1;

		printf("\t%d Page Table Entries | Page Table Index %d\n", pObj->num_page_table_entries, pObj->page_table_index);

		NumberPageTableEntries = pObj->num_page_table_entries;
		pDst = pObject;
		
		for(PageNumber = 0; PageNumber < NumberPageTableEntries; PageNumber++, pObjPage++){
			
			PBYTE pSrc = OriginalBuffer + pLX->data_pages_offset + (pObjPage->page_data_offset << pLX->page_offset_shift);
			
			switch(pObjPage->flags){
				
				case 0x00:
					printf("\t\tCopying %d bytes from %p (%p) to %p\n", pObjPage->data_size, pSrc, pSrc - OriginalBuffer, pDst);
					memcpy(TranslateEmulatedToVirtualAddress(pDst), pSrc, pObjPage->data_size);
					//memcpy((pDst), pSrc, pObjPage->data_size);
					if(pObjPage->data_size < pLX->page_size){
						memset(TranslateEmulatedToVirtualAddress(pDst) + pObjPage->data_size, 0, pLX->page_size - pObjPage->data_size);
					}
					break;
				default:
					fprintf(stderr, "Unknown flag 0x%x in object %d page %d\n", pObjPage->flags, i, PageNumber);
					exit(0);
			}
			
			//fixups & imports
			FixupPage(pLX, pObj, pDst);
			
			pDst += pLX->page_size;
			
		}
		
		//zero out remaining areas
		
		RemainingBytes = ObjectVirtualSize - (DWORD)(pDst - pObject);
		//memset(pDst, 0, RemainingBytes);
		memset(TranslateEmulatedToVirtualAddress(pDst), 0, RemainingBytes);

	}
	
	//does any of this matter for libraries? (or at all, in the grand scheme)
	
	//All pages in the EXE have been loaded into memory
	
	var_eip = pLX->eip;
	
	if(pLX->eip_object){ //reloc base address is the address that it has been relocated to
		LxObjectTableEntry* pTargetObj = (LxObjectTableEntry*)(Buffer + pLX->object_table_offset) + (pLX->eip_object - 1);
		var_eip += pTargetObj->reloc_base_addr;
	}
	
	var_esp = pLX->esp;
	
	if(var_esp == 0){
		fprintf(stderr, "ESP==0!!!!\n");
		exit(0);
	}
	
	if(pLX->esp_object){
		LxObjectTableEntry* pTargetObj = (LxObjectTableEntry*)(Buffer + pLX->object_table_offset) + (pLX->esp_object - 1);
		var_esp += pTargetObj->reloc_base_addr;
	}
	
	pEnvironment = "TEST=abc\0\0hello.exe\0hello.exe\0\0";
	pCmdLine = pEnvironment + 10;
	
	//call into the OS/2 binary
	printf("Jumping into OS/2 land! EIP = %p ESP = %p\n", var_eip, var_esp);
	
	return LaunchOS2(var_eip, var_esp, pEnvironment, pCmdLine, 0);
}

int main(int argc, char** argv){
	PSTR ExeName;
	DWORD ExeSize;
	FILE* fp;
	PBYTE ExeBuffer;

	if(argc < 2){
		fprintf(stderr, "Please specify an EXE file to load on the command line.\n");
		return 1;
	}

	ExeName = argv[1];
	fp = fopen(ExeName, "rb");

	if(!fp){
		fprintf(stderr, "Can't load %s\n", ExeName);
		return 1;
	}

	
	if(fseek(fp, 0, SEEK_END) < 0){
		fprintf(stderr, "Unable to seek file\n");
		return 1;
	}

	ExeSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);  

	ExeBuffer = malloc(ExeSize);

	if(!ExeBuffer){
		fprintf(stderr, "No memory!\n");
		return 1;
	}

	if(fread(ExeBuffer, ExeSize, 1, fp) != 1){
		fprintf(stderr, "Failed to read EXE contents\n");
		return 1;
	}

	fclose(fp);

	printf("Loading %s\n", ExeName);
	
	init_386(&cpu);
	
	VMMInit();
	
	printf("Program returned: %p\n", LoadLX(ExeName, ExeBuffer, ExeSize));

	return 0;
}
