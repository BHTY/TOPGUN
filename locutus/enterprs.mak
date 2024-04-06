!include <ntwin32.mak>

all: enterprs.exe

enterprs.exe: enterprs.obj os2mem.obj os2thunk.obj doscalls.obj ..\maverick\run386.obj ..\maverick\wnd.obj ..\iceman\dis.obj
  $(link) /base:0x800000 /SUBSYSTEM:CONSOLE,3.10 $(ldebug) /debugtype:coff $(conlflags) -out:$*.exe $** $(conlibs) user32.lib gdi32.lib

{src}.c{src}.obj:
  $(CC) /O2 /Zi /FA /nologo $(CFLAGS) /Z7 /c /Fosrc\ $<
