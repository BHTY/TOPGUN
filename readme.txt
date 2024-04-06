Todo List

MAVERICK
- Get the remainder of the 8086 CPU tests to work

ICEMAN
- If there's any way to emit symbol information from NASM for COM files, an ability to load that would be cool

BOTH
- Open the debugger pipe in asynchronous mode to let breakpoints work better
- Get data breakpoints working
- Flesh out the debugger some more... stack traces, etc.

OTHER
- Load 32-bit OS/2 LX binaries into the virtual memory of the CPU emulator... (and don't worry too much about segmentation just yet)
	For now, manually resolve any imports (i.e. DosPutMessage)...
		The way I'll handle thunks for now is this:
			THUNK_DOSPUTMESSAGE:
				MOV EAX, HostAddress_pDosPutMessage
				INT 0x80
				RET
		A table will be maintained of import IDs to thunk addresses. When a recognized import is made, the thunk will be generated in emu-memory, 
		referencing the desired thunk address (a function in host memory). INT 0x80 transfers the arguments from the emulated stack to the host's
		stack, invokes the desired function, and then stores the value in the emulated EAX register.


Helpful macros will be used to help define thunk functions, which will automatically "transform" emulated pointers to host pointers.

USHORT DosPutMessage(USHORT hFile, USHORT msgLen, PCHAR msgBuf);