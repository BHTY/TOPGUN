#pragma once
#include "os2mem.h"
typedef uint16_t APIRET;
typedef uint16_t OS2HFILE;

#define TranslateHFILE(hFile)	(hFile)
#define ALIGN(x, sz)	(((x % sz) == 0) ? x : (x + sz) - (x % sz))

#define TRACE(x)		printf x

/*#define NO_ERROR 0
#define ERROR_INVALID_PARAMETER 87
#define ERROR_BUFFER_OVERFLOW 111*/

OS2PTR32 GetModuleProcAddrByOrdinal(WORD ModuleId, DWORD ImportId);

extern PVOID ModuleThunkTable[256];
extern PVOID MsgImportThunks[];
extern PVOID DosImportThunks[];