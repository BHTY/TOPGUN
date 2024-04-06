#pragma once
#include "os2mem.h"
typedef uint16_t APIRET;
typedef uint16_t OS2HFILE;

#define TranslateHFILE(hFile)	(hFile)
#define ALIGN(x, sz)	(((x % sz) == 0) ? x : (x + sz) - (x % sz))

#define TRACE(a)		printf(a)

OS2PTR32 GetModuleProcAddrByOrdinal(WORD ModuleId, DWORD ImportId);

extern PVOID ModuleThunkTable[256];
extern PVOID MsgImportThunks[];
extern PVOID DosImportThunks[];