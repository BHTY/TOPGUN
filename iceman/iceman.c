#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <windows.h>
#include "../common/cmd.h"
#include "../common/i386.h"

void set_title(char* pipename){
	char str[80];
	sprintf(str, "ICEMAN-386 connected on %s", pipename);
	SetConsoleTitle(str);
}

HANDLE OpenConnection(char* pipename){
	HANDLE hPipe;
	printf("Opening pipe %s to MAVERICK\n", pipename);
	
	while(1){
	
		hPipe = CreateFileA(pipename, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
		
		if(hPipe != INVALID_HANDLE_VALUE) break;
		if(GetLastError() != ERROR_PIPE_BUSY) return 0;
		if(!WaitNamedPipeA(pipename, 20000)){
			return 0;
		}
	}
	
	set_title(pipename);
	
	return hPipe;
}

void make_str_lower(char* buf, int len){
	while(len--){
		*buf = *buf > 0x40 && *buf < 0x5b ? *buf | 0x60 : *buf;
		buf++;
	}
}

void dump_regs_386(ice_regs* pRegs) {
	int i;

	printf("EIP= %p CPL=%d EFLAGS= ", pRegs->eip, pRegs->cpl);
	//dump_eflags_i386(pCPU);

	for (i = 0; i < 8; i++) {
		if ((i % 4) == 0) printf("\n");

		printf("%s: %p  ", regs_32[i], pRegs->regs[i]);
	}

	printf("\n");
}

int WaitPipeResponse(HANDLE hPipe, PVOID pBuf, int max_size, int min_size){
	int res;
	int received;
	
	while(1){
		res = ReadFile(hPipe, pBuf, max_size, &received, NULL);
		
		if(res && received >= min_size) break;
	}
	
	return received;
}

void SendBreakMessage(HANDLE hPipe, brk_resp* csip){
	int num_bytes;
	cmd_pkt pkt;
	pkt.cmd = BREAK;
	WriteFile(hPipe, &pkt, sizeof(cmd_pkt), &num_bytes, NULL);
	WaitPipeResponse(hPipe, csip, sizeof(brk_resp), sizeof(brk_resp));
	printf("Broken at %04x:%08x.\n\n", csip->cs, csip->ip);
}

typedef struct{
	HANDLE hPipe;
	brk_resp* csip;
	int complete;
} wait_struct;

DWORD WINAPI WaitForBreakMsgThread(wait_struct* pWait){
	WaitPipeResponse(pWait->hPipe, pWait->csip, sizeof(brk_resp), sizeof(brk_resp));
	pWait->complete = 1;
}

void GoUntilBP(HANDLE hPipe, brk_resp* csip){
	int res, received;
	wait_struct waiting;
	HANDLE hThread;
	
	waiting.hPipe = hPipe;
	waiting.csip = csip;
	waiting.complete = 0;
	
	hThread = CreateThread(NULL, 0, WaitForBreakMsgThread, &waiting, 0, NULL);
	
	while(1){
		if(waiting.complete){
			printf("Broken at %04x:%08x.\n\n", csip->cs, csip->ip);
			break;
		}
		
		if(kbhit()){
			getch();
			TerminateThread(hThread, 0);
			SendBreakMessage(hPipe, csip);
			break;
		}
	}
}

int main(int argc, char** argv){
	HANDLE hPipe = OpenConnection(argv[1]);
	int num_bytes;
	char buffer[512];
	char cmd[50];
	cmd_pkt pkt;
	ice_regs regs;
	brk_resp csip;
	step_pkt step;
	int i, p;
	
	if(!hPipe){
		printf("Failed to open named pipe\n");
		return -1;
	}
	
	SendBreakMessage(hPipe, &csip);
	
	while(1){
		cmd[0] = 0;
		printf("%04x:%08x> ", csip.cs, csip.ip);
		fgets(buffer, 511, stdin);
		//WriteFile(hPipe, buffer, strlen(buffer) + 1, &num_bytes, NULL);
		
		make_str_lower(buffer, strlen(buffer));
		
		if(sscanf(buffer, "%s", cmd) == 0 || strlen(buffer) == 0){
			continue;
		}		
		
		if(strcmp(cmd, "\n") == 0){
			continue;
		}
		
		if(strcmp(cmd, "r") == 0){
			pkt.cmd = DUMP_REGS;
			WriteFile(hPipe, &pkt, sizeof(cmd_pkt), &num_bytes, NULL);
			//wait for response and print
			WaitPipeResponse(hPipe, &regs, sizeof(ice_regs), sizeof(ice_regs));
			dump_regs_386(&regs);
			printf("\n");
			
		}else if(strcmp(cmd, "d") == 0){
			pkt.cmd = DUMP_MEMORY;
			sscanf(buffer, "%s %x %x", cmd, &(pkt.args[0]), &(pkt.args[1]));
			WriteFile(hPipe, &pkt, sizeof(cmd_pkt), &num_bytes, NULL);
			//wait for response and print
			
		}else if(strcmp(cmd, "dmf") == 0){
			printf("Unsupported!\n");
			
		}else if(strcmp(cmd, "lfm") == 0){
			printf("Unsupported!\n");
			
		}else if(strcmp(cmd, "dgdt") == 0){
			printf("Unsupported!\n");
			
		}else if(strcmp(cmd, "didt") == 0){
			printf("Unsupported!\n");
			
		}else if(strcmp(cmd, "u") == 0){
			printf("Unsupported!\n");
			
		}else if(strcmp(cmd, "bp") == 0){
			pkt.cmd = SET_BP;
			sscanf(buffer, "%s %x", cmd, &(pkt.args[0]));
			WriteFile(hPipe, &pkt, sizeof(cmd_pkt), &num_bytes, NULL);
			//get breakpoint number response and print
			
		}else if(strcmp(cmd, "br") == 0){
			pkt.cmd = UNSET_BP;
			sscanf(buffer, "%s %x", cmd, &(pkt.args[0]));
			WriteFile(hPipe, &pkt, sizeof(cmd_pkt), &num_bytes, NULL);
			//get breakpoint number response and print
			
		}
		else if(strcmp(cmd, "wp") == 0){
			pkt.cmd = SET_WP;
			if(sscanf(buffer, "%s %x %x", cmd, &(pkt.args[0]), &(pkt.args[1])) < 3){
				pkt.args[1] = pkt.args[0]+1;
			}
			WriteFile(hPipe, &pkt, sizeof(cmd_pkt), &num_bytes, NULL);
			//get breakpoint number response and print
		}
		else if(strcmp(cmd, "wr") == 0){
			pkt.cmd = UNSET_WP;
			sscanf(buffer, "%s %x", cmd, &(pkt.args[0]));
			WriteFile(hPipe, &pkt, sizeof(cmd_pkt), &num_bytes, NULL);
			//get breakpoint number response and print
		}
		else if(strcmp(cmd, "bl") == 0){
			pkt.cmd = LIST_BP;
			WriteFile(hPipe, &pkt, sizeof(cmd_pkt), &num_bytes, NULL);
			//get return list and print
			
		}
		else if(strcmp(cmd, "wl") == 0){
			pkt.cmd = LIST_WP;
			WriteFile(hPipe, &pkt, sizeof(cmd_pkt), &num_bytes, NULL);
			//get return list and print
		}
		else if(strcmp(cmd, "g") == 0){
			if(sscanf(buffer, "%s %x:%x", cmd, &(pkt.args[0]), &(pkt.args[1])) < 3){
				pkt.args[0] = -1;
				
				if(sscanf(buffer, "%s %x", cmd, &(pkt.args[1])) < 2){
					pkt.args[1] = -1;
				}
			}
			pkt.cmd = GO;
			WriteFile(hPipe, &pkt, sizeof(cmd_pkt), &num_bytes, NULL);
			GoUntilBP(hPipe, &csip);
		}
		else if(strcmp(cmd, "t") == 0){
			if(sscanf(buffer, "%s %x", cmd, &(pkt.args[0])) < 2){
				pkt.args[0] = 1;
			}
			
			for(p = 0; p < pkt.args[0]; p++){
			
				printf("%04x:%08x: ", csip.cs, csip.ip);
				
				pkt.cmd = TRACE;
				WriteFile(hPipe, &pkt, sizeof(cmd_pkt), &num_bytes, NULL);
				WaitPipeResponse(hPipe, &step, sizeof(step_pkt), sizeof(step_pkt));
				csip.cs = step.csip.cs;
				csip.ip = step.csip.ip;
				
				for(i = 0; i < step.num_bytes; i++){
					printf("%02x ", step.bytes[i]);
				}
				
				printf("\n");
			}
			
			printf("\n");
			
			//disassemble
		}
		else if(strcmp(cmd, "p") == 0){
			printf("Unsupported!\n");
		}
		else if(strcmp(cmd, "o") == 0){
			printf("Unsupported!\n");
		}else if(strcmp(cmd, "q") == 0){
			break;
		}
		else{
			printf("ICE-MANAGER 386 VERSION 0.1\n");
			printf("All numbers and addresses are given in hexadecimal. If a far pointer is requested but the segment is omitted, the current code segment is implied. If the entire pointer is omitted, the current CS:IP is implied. Press Q to quit.\n\n");
			
			printf("Dumping Program Status Commands\n");
			printf("-------------------------------\n");
			printf("R: Dump registers\n");
			printf("D ADDR(LINADDR) PARAGRAPHS(INT): Dumps memory in 16-byte paragraphs from the linear address\n");
			printf("DMF ADDR(LINADDR) BYTES(INT) FILENAME(STRING): Dumps BYTES memory from the linear address into the indicated file\n");
			printf("LFM ADDR(LINADDR) BYTES(INT) FILENAME(STRING): Loads BYTES memory from the indicated file into the linear address\n");
			printf("E ADDR(LINADDR) bytes: Enters comma-separated list of bytes into the specified linear address\n");
			printf("DGDT: Dumps the Global Descriptor Table\n");
			printf("DLDT: Dumps the Local Descriptor Table\n");
			printf("DIDT: Dumps the Interrupt Vector Table (Real Mode) / Interrupt Descriptor Table (Protected Mode)\n");
			printf("U SEG:OFFS(FARPTR) BYTES(INT): Disassembles BYTES from the indicated far code address.\n");
			printf("\n");
			
			printf("Breakpoints\n");
			printf("-----------\n");
			printf("BP ADDR(LINADDR): Sets a code breakpoint at the given linear address\n");
			printf("BR ADDR(LINADDR): Removes the code breakpoint at the given linear address\n");
			printf("WP ADDR(LINADDR) END(LINADDR): Sets a data breakpoint at the given linear start address until the given end address. If no end address is specified, it is implied to be one byte after the start.\n");
			printf("WR ADDR(LINADDR): Removes the data breakpoint at the given linear address\n");
			printf("BL: Lists code breakpoints\n");
			printf("WL: Lists data breakpoints\n");
			printf("\n");
			
			printf("Execution Control\n");
			printf("-----------------\n");
			printf("T: Single-step\n");
			printf("P: Step over (break after function call)\n");
			printf("O: Step out (continue until the current function returns)\n");
			printf("G SEG:OFFS(FARPTR): Goes until breakpoint (or until hotkey is struck)\n\n");
		}
		
		/*switch(cmd){
			case 'r': //wait for register dumps
				pkt.cmd = 2;
				WriteFile(hPipe, &pkt, sizeof(pkt), &num_bytes, NULL);
				break;
			case 'g':
				pkt.cmd = 1;
				WriteFile(hPipe, &pkt, sizeof(pkt), &num_bytes, NULL);
				break;
			case 'b':
				pkt.cmd = 0;
				WriteFile(hPipe, &pkt, sizeof(pkt), &num_bytes, NULL);
				break;
		}*/
	}
	
}