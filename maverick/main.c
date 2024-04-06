#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "run386.h"
#include <windows.h>
#include "wnd.h"
#include "../common/cmd.h"
#include <setjmp.h>

#define ESCAPE_ADDRESS 0x0000BEEF

char* Os2Base;

int ice_running = 1;

int signal_break = 0;

HWND hDosWindow;
HANDLE globalPipe;

breakpoint code_breakpoints[NUM_BPS] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
breakpoint data_breakpoints[NUM_BPS] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}};

#define IN_BP(bp, addr)			(bp.enabled && (addr >= bp.address) && (addr < bp.end))

int HostCall(int eax, int esp, int num){
	printf("UNSUPPORTED\n");
	return 0;
}

void RemoveBreakpoint(uint32_t addr){
	int i;
	
	for(i = 0; i < NUM_BPS; i++){
		if(code_breakpoints[i].address == addr){
			code_breakpoints[i].enabled = 0;
		}
	}
}

int AddBreakpoint(uint32_t addr){
	int i;
	
	for(i = 0; i < NUM_BPS; i++){
		if(code_breakpoints[i].enabled == 0){
			code_breakpoints[i].address = addr;
			code_breakpoints[i].end = addr + 1;
			code_breakpoints[i].enabled = 1;
			
			return i;
		}
	}
	
	return -1;
}

i386* Pcpu;

uint8_t* TranslateEmulatedToVirtualAddress(uint32_t addr) {
	
	return Os2Base + (addr & 0xFFFFF);
}

uint32_t io_read_32(uint16_t port) {
	return 0xfc;
}

uint32_t io_read_16(uint16_t port) {
	return io_read_32(port) & 0xFFFF;
}

uint32_t io_read_8(uint16_t port) {
	return io_read_32(port) & 0xFF;
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
	/*if(addr >= 0xa0000){
		//dump_386(Pcpu);
		printf("Drawing %04x to %05x (IP=%04x)\n", value, addr, Pcpu->ip);
		
		if(Pcpu->ip > 0x2d0) dump_386(Pcpu);
	}*/
	
	*(uint16_t*)(TranslateEmulatedToVirtualAddress(addr)) = value;
}

void bus_write_32(uint32_t addr, uint32_t value) {
	*(uint32_t*)(TranslateEmulatedToVirtualAddress(addr)) = value;
}

void init_386(i386* pCPU) {
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

	pCPU->es.selector = 1*0x7C0;
	pCPU->fs.selector = 1*0x7C0;
	pCPU->gs.selector = 1*0x7C0;
	pCPU->ds.selector = 1*0x7C0;
	pCPU->ss.selector = 1*0x7C0;
	pCPU->cs.selector = 1*0x7C0;

	pCPU->es.base = 1*0x7C00;
	pCPU->fs.base = 1*0x7C00;
	pCPU->gs.base = 1*0x7C00;
	pCPU->ds.base =1* 0x7C00;
	pCPU->ss.base = 1*0x7C00;
	pCPU->cs.base = 1*0x7C00;

	//pCPU->cs.flags = 0x4;
	//pCPU->ss.flags = 0x4;

}

/*
	EmuICE-386 Commands
All memory addresses are given as linear physical addresses.
	
D <MEM> <PARAGRAPHS>: Dumps memory starting at MEM			*
G: Goes														*
T: Traces													*
B <MEM>: Sets breakpoint at MEM
U <MEM>: Unsets breakpoint at MEM
W <MEM>: Sets watchpoint for address MEM
N <MEM>: Removes watchpoint for address MEM
R: Dumps registers											*
*/

uint32_t old_eip;
int single_step = 0;

int ice_step(i386* pCPU) {
	char input_string[512];
	int res;
	char cmd;
	uint32_t arg1, arg2;
	int i, p;
	int ret_val;
	uint32_t linaddr;
	brk_resp csip;

	old_eip = pCPU->eip;
	
	linaddr = pCPU->cs.base + pCPU->eip;
	
	while(!(pCPU->running));
	
	if(IN_BP(code_breakpoints[0], linaddr) || IN_BP(code_breakpoints[1], linaddr) || IN_BP(code_breakpoints[2], linaddr) || IN_BP(code_breakpoints[3], linaddr)){
		//printf("%d, %d, %d, %d\n", IN_BP(code_breakpoints[0], linaddr), IN_BP(code_breakpoints[1], linaddr), IN_BP(code_breakpoints[2], linaddr), IN_BP(code_breakpoints[3], linaddr));
		
		csip.ip = pCPU->eip;
		csip.cs = pCPU->cs.selector;
			
		printf("Breakpoint hit at %04x:%08x\n", csip.cs, csip.ip);
			
		WriteFile(globalPipe, &csip, sizeof(brk_resp), &ret_val, NULL);
		
		pCPU->running = 0;
		return;
	}

	if (single_step) {

		printf("%04x:%08x: ", pCPU->cs.selector, old_eip);
		ret_val = i386_step(pCPU);

		for (; old_eip < pCPU->old_eip; old_eip++) {
			printf("%02x ", bus_read_8(pCPU->cs.base + old_eip));
		}

		if (getchar() == 'p') {
			dump_386(pCPU);
		}

		//printf("\n\t\t\tAX=%04x CX=%04x SI=%04x DI=%04x\n", pCPU->ax, pCPU->cx, pCPU->si, pCPU->di);
		printf("\n");
		return ret_val;
	}
	else {
	//	if (pCPU->eip == 0x7c49) single_step = 1;
		if (pCPU->eip == 0x1a7) {
			//single_step = 1;
		}

		if (0 && pCPU->eip == 0x7d65) {
			printf("CMP AX (%04x), 0xCE20", pCPU->ax);

			ret_val = i386_step(pCPU);

			printf(" ZF = %d\n", GET_FLAG(ZERO));
		}
		else {
			ret_val = i386_step(pCPU);
		}

		return ret_val;
	}

	if (ice_running) {
		if (pCPU->eip == 0x7c1c) {
			ice_running = 0;
			return -1;
		}

		return i386_step(pCPU);
	}
	else {
		printf("ice> ");
		fgets(input_string, 511, stdin);
		res = sscanf(input_string, "%c %x %x", &cmd, &arg1, &arg2);

		switch (cmd) {
			case 'r':
			case 'R':
				dump_386(pCPU);
				ret_val = -1;
				break;
			case 'd':
			case 'D':
				for (i = 0; i < arg2; i++) {
					printf("%p: ", i * 16 + arg1);

					for (p = 0; p < 16; p++) {
						printf("%02x ", bus_read_8(arg1 + i * 16 + p));
					}

					printf(" | ");

					for (p = 0; p < 16; p++) {
						printf("%c", bus_read_8(arg1 + i * 16 + p));
					}

					printf("\n");
				}

				ret_val = -1;
				break;
			case 't':
			case 'T':
				printf("%04x:%08x: ", pCPU->cs.selector, old_eip);
				ret_val = i386_step(pCPU);

				for (; old_eip < pCPU->old_eip; old_eip++) {
					printf("%02x ", bus_read_8(pCPU->cs.base + old_eip));
				}
				printf("\n");
				break;
			case 'G':
			case 'g':
				if (res == 2) {
					pCPU->eip = arg1;
				}
				ret_val = -1;
				ice_running = 1;
				break;
			default:
				printf("Unrecognized command\n");
				ret_val = -1;
				break;
		}
	}

	return ret_val;

	
}

uint32_t Exec386Subroutine_Near32(i386* pCPU, uint32_t dwTargetAddress, uint32_t* dwParamList, uint32_t nParams) {
	int i;
	uint32_t old_eip;

	uint32_t dwReturnAddress = pCPU->eip;

	for (i = nParams - 1; i >= 0; i--) {
		i386_push(pCPU, dwParamList[i]);
	}

	i386_push(pCPU, ESCAPE_ADDRESS);

	pCPU->eip = dwTargetAddress;

	while (pCPU->eip != ESCAPE_ADDRESS) {
		old_eip = pCPU->eip;

		//printf("%p (%04x): \n", old_eip, pCPU->edi);
		
		//InvalidateRect(hDosWindow, NULL, 0);
		//ProcessWindowEvents(hDosWindow);


		//printf("%04x:%04x: ", pCPU->cs.selector, pCPU->ip);
		
		ice_step(pCPU);

		for (; old_eip < pCPU->old_eip; old_eip++) {
			//printf("%02x ", bus_read_8(pCPU->cs.base + old_eip));
		}

		//printf("\n");
		
		
		
		
		//getchar();

		//printf("ESP = %p and DWORD[ESP] = %p\n", pCPU->esp, bus_read_32(pCPU->ss.base + pCPU->esp));

		//printf("\n");
		//dump_386(pCPU);

		//getchar();
	}

	//pop data off of stack
	dump_386(pCPU);
	 
	return pCPU->eax;
}

//char program[] = { 0xB8, 0x04, 0x00, 0x00, 0x00, 0xBB, 0x07, 0x00, 0x00, 0x00, 0x50, 0x53, 0xE8, 0x04, 0x00, 0x00, 0x00, 0x83, 0xC0, 0x01, 0xC3, 0x8B, 0x44, 0x24, 0x04, 0x8B, 0x5C, 0x24, 0x08, 0x01, 0xD8, 0xC2, 0x08, 0x00 };
//char program[] = { 0xB8, 0x01, 0x00, 0x00, 0x00, 0xBB, 0x02, 0x00, 0x00, 0x00, 0x39, 0xD8, 0x74, 0x08, 0x75, 0x0C, 0xB8, 0x05, 0x00, 0x00, 0x00, 0xC3, 0xB8, 0x06, 0x00, 0x00, 0x00, 0xC3, 0xB8, 0x07, 0x00, 0x00, 0x00, 0xC3 };
char program[] = {  0x66, 0xB8, 0xFF, 0xFF, 0xC3 } ;

DWORD WINAPI ICEThread(HANDLE hPipe){
	char buffer[1024];
	int num_bytes;
	
	int res;
	cmd_pkt pkt;
	decoded_op dst;
	ice_regs regs;
	brk_resp csip;
	step_pkt step;
	uint32_t old_eip;
	int i;
	char* temp_buf;
	
	ConnectNamedPipe(hPipe, NULL);
	
	printf("Debugger connected\n");
	
	while(1){
		res = ReadFile(hPipe, buffer, 1024, &num_bytes, NULL);
		
		if(!res){
			if(GetLastError() == ERROR_BROKEN_PIPE){
				DisconnectNamedPipe(hPipe);
				printf("Debugger disconnected\n");
				ConnectNamedPipe(hPipe, NULL);
				printf("Debugger connected\n");
			}
		}
		
		if(res){
			if(num_bytes >= sizeof(cmd_pkt)){
				memcpy(&pkt, buffer, sizeof(cmd_pkt));
				
				switch(pkt.cmd){
					case GO:
						printf("Going to address %x:%x\n", pkt.args[0], pkt.args[1]);
						
						dst.type = SEG_REG;
						dst.id = CS;
						dst.size = SZ_16;
						
						if(pkt.args[0] != -1){ //the segment is valid
							load_seg_reg(Pcpu, &dst, pkt.args[0]);
						}
						
						if(pkt.args[1] != -1){ //the offset is valid
							Pcpu->eip = pkt.args[1];
						}
						
						Pcpu->running = 1;
						break;
					case BREAK:
						Pcpu->running = 0;
						
						csip.ip = Pcpu->eip;
						csip.cs = Pcpu->cs.selector;
						
						WriteFile(hPipe, &csip, sizeof(brk_resp), &num_bytes, NULL);
						
						printf("Breaking at %04x:%08x\n", csip.cs, csip.ip);
						
						break;
					case TRACE:
						old_eip = Pcpu->eip;
						i386_step(Pcpu);
						step.num_bytes = Pcpu->old_eip - old_eip;
						step.csip.cs = Pcpu->cs.selector;
						step.csip.ip = Pcpu->ip;
						
						for(i = 0; i < step.num_bytes; i++){
							step.bytes[i] = bus_read_8(Pcpu->cs.base + old_eip + i);
						}
						
						WriteFile(hPipe, &step, sizeof(step_pkt), &num_bytes, NULL);
						break;
					case DUMP_REGS:
						memcpy(regs.regs, Pcpu->regs, 8 * 4);
						memcpy(regs.cr, Pcpu->cr, 9 * 4);
						memcpy(regs.tr, Pcpu->tr, 8 * 4);
						memcpy(regs.dr, Pcpu->dr, 8 * 4);
						regs.eip = Pcpu->eip;
						regs.eflags = Pcpu->eflags;
						memcpy(regs.seg_regs, Pcpu->seg_regs, 6 * sizeof(SEGREG));
						regs.gdtr = Pcpu->gdtr;
						regs.ldtr = Pcpu->ldtr;
						regs.idtr = Pcpu->idtr;
						regs.taskr = Pcpu->taskr;
						regs.cpl = Pcpu->cpl;
					
						WriteFile(hPipe, &regs, sizeof(ice_regs), &num_bytes, NULL);
						break;
					case DUMP_MEMORY:
						temp_buf = malloc(pkt.args[1] * 16);
						
						for(i = 0; i < pkt.args[1] * 16; i++){
							temp_buf[i] = bus_read_8(pkt.args[0] + i);
						}
						
						WriteFile(hPipe, temp_buf, pkt.args[1] * 16, &num_bytes, NULL);
						free(temp_buf);
						break;
						
					case LIST_BP:
						//printf("Received command to list breakpoints\n");
						WriteFile(hPipe, code_breakpoints, sizeof(code_breakpoints), &num_bytes, NULL);
						break;
						
					case LIST_WP:
						WriteFile(hPipe, data_breakpoints, sizeof(data_breakpoints), &num_bytes, NULL);
						break;
						
					case SET_BP:
						old_eip = AddBreakpoint(pkt.args[0]);
					
						WriteFile(hPipe, &old_eip, 4, &num_bytes, NULL);
						break;
						
					case UNSET_BP:
						RemoveBreakpoint(pkt.args[0]);
						break;
						
					default:
						break;
				}
			}
		}
	}
}

int main(int argc, char** argv) {
	int sz;
	i386 cpu;
	char* stack = malloc(4096);
	FILE* fp = fopen(argv[1], "rb");
	HANDLE hPipe;

	init_386(&cpu);
	
	Pcpu = &cpu;
	
	if(argc > 2){
		hPipe = CreateNamedPipeA(argv[2], PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE, 2, 4096, 4096, 0, NULL);
		if(hPipe && hPipe != INVALID_HANDLE_VALUE){
			globalPipe = hPipe;
			printf("Creating named pipe %s with handle %p\n", argv[2], hPipe);
			CreateThread(NULL, 0, ICEThread, hPipe, 0, NULL);
			Pcpu->running = 0;
			printf("Awaiting debugger connection to continue.\n");
		}else{
			printf("Failed to create named pipe with name %s\n", argv[2]);
		}
	}

	/*cpu.esp = stack + 4096;

	dump_386(&cpu);

	printf("Subroutine returned: %p\n", Exec386Subroutine_Near32(&cpu, program, NULL, 0));

	printf("Hello, world!\n");*/

	Os2Base = malloc(1024 * 1024);
	memset(Os2Base, 0, 1024 * 1024);
	
	fseek(fp, 0, SEEK_END);
	sz = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	fread(Os2Base + 0x7C00 + 0x100, 1, sz, fp);

	cpu.esp = 0xFFFE;
	
	hDosWindow = InitDOSWindow();

	//dump_386(&cpu);

	Exec386Subroutine_Near32(&cpu, 0x100, NULL, 0);
}