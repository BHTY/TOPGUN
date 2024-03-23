!include <ntwin32.mak>

all: iceman.exe

iceman.exe: iceman.obj
  $(link) /base:0x800000 /SUBSYSTEM:CONSOLE,3.10 $(ldebug) /debugtype:coff $(conlflags) -out:$*.exe $** $(conlibs) GDI32.lib USER32.lib

{src}.c{src}.obj:
  $(CC) /O2 /Zi /FA /nologo $(CFLAGS) /Z7 /c /Fosrc\ $<
