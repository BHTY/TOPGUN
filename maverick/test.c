#include "../common/i386.h"
#include "run386.h"
#include <stdio.h>
#include <stdlib.h>

/*
	The bug is bc bp-relative/sp-relative invokes ss instead of ds
*/

extern char* Os2Base;

unsigned short load_word(FILE* fp) {
	unsigned short temp;
	fread(&temp, 2, 1, fp);
	return temp;
}

void load_regs(FILE* fp, i386* pCPU) {
	fread(&(pCPU->ax), 2, 1, fp);
	fread(&(pCPU->bx), 2, 1, fp);
	fread(&(pCPU->cx), 2, 1, fp);
	fread(&(pCPU->dx), 2, 1, fp);

	//load CS, SS, DS, ES
	fread(&(pCPU->cs.selector), 2, 1, fp);
	pCPU->cs.base = pCPU->cs.selector << 4;
	fread(&(pCPU->ss.selector), 2, 1, fp);
	pCPU->ss.base = pCPU->ss.selector << 4;
	fread(&(pCPU->ds.selector), 2, 1, fp);
	pCPU->ds.base = pCPU->ds.selector << 4;
	fread(&(pCPU->es.selector), 2, 1, fp);
	pCPU->es.base = pCPU->es.selector << 4;

	pCPU->es.access = 0x80 | 0x10 | 0x2;
	pCPU->fs.access = 0x80 | 0x10 | 0x2;
	pCPU->gs.access = 0x80 | 0x10 | 0x2;
	pCPU->ds.access = 0x80 | 0x10 | 0x2;
	pCPU->ss.access = 0x80 | 0x10 | 0x2;
	pCPU->cs.access = 0x80 | 0x10 | 0x8 | 0x2;

	//load SP, BP, SI, DI, IP, FLAGS
	fread(&(pCPU->sp), 2, 1, fp);
	fread(&(pCPU->bp), 2, 1, fp);
	fread(&(pCPU->si), 2, 1, fp);
	fread(&(pCPU->di), 2, 1, fp);
	fread(&(pCPU->ip), 2, 1, fp);
	fread(&(pCPU->flags), 2, 1, fp);
}

void load_mem(FILE* fp) {
	int num = fgetc(fp);
	int i;
	unsigned int addr;
	unsigned char val;

	for (i = 0; i < num; i++) {
		fread(&addr, 4, 1, fp);
		fread(&val, 1, 1, fp);
		bus_write_8(addr, val);
		//printf("Stored %d at %d\n", val, addr);
	}
}

#define SPCL_PRINT_FLAG(num, mask, set, clear)   if(num & (1 << mask)){ \
												fprintf(stdout, set); \
											} else { \
												fprintf(stdout, clear); \
											}

void print_flags(unsigned int flags) {
	SPCL_PRINT_FLAG(flags, V8086, "V", "");
	SPCL_PRINT_FLAG(flags, RESUME, "R", "");
	SPCL_PRINT_FLAG(flags, NESTED, "N", "");
	SPCL_PRINT_FLAG(flags, OVERFLOW, "O", "");
	SPCL_PRINT_FLAG(flags, DIRECTION, "D", "d");
	SPCL_PRINT_FLAG(flags, INTERRUPT, "I", "");
	SPCL_PRINT_FLAG(flags, TRAP, "T", "");
	SPCL_PRINT_FLAG(flags, SIGN, "S", "");
	SPCL_PRINT_FLAG(flags, ZERO, "Z", "");
	SPCL_PRINT_FLAG(flags, AC, "A", "");
	SPCL_PRINT_FLAG(flags, PARITY, "P", "");
	SPCL_PRINT_FLAG(flags, CARRY, "C", "");
}

int check_regs(FILE* fp, i386* pCPU) {
	unsigned short val;
	int failed = 0;

	val = load_word(fp);

	if (pCPU->ax != val) {
		fprintf(stdout, "AX: Correct Value= %04x Actual Value= %04x\n", val, pCPU->ax);
		failed= 1;
	}

	val = load_word(fp);

	if (pCPU->bx != val) {
		fprintf(stdout, "BX: Correct Value= %04x Actual Value= %04x\n", val, pCPU->bx);
		failed= 1;
	}

	val = load_word(fp);

	if (pCPU->cx != val) {
		fprintf(stdout, "CX: Correct Value= %04x Actual Value= %04x\n", val, pCPU->cx);
		failed= 1;
	}

	val = load_word(fp);

	if (pCPU->dx != val) {
		fprintf(stdout, "DX: Correct Value= %04x Actual Value= %04x\n", val, pCPU->dx);
		failed= 1;
	}

	val = load_word(fp);

	if (pCPU->cs.selector != val) {
		fprintf(stdout, "CS: Correct Value= %04x Actual Value= %04x\n", val, pCPU->cs.selector);
		failed= 1;
	}

	val = load_word(fp);

	if (pCPU->ss.selector != val) {
		fprintf(stdout, "SS: Correct Value= %04x Actual Value= %04x\n", val, pCPU->ss.selector);
		failed= 1;
	}

	val = load_word(fp);

	if (pCPU->ds.selector != val) {
		fprintf(stdout, "DS: Correct Value= %04x Actual Value= %04x\n", val, pCPU->ds.selector);
		failed= 1;
	}

	val = load_word(fp);

	if (pCPU->es.selector != val) {
		fprintf(stdout, "ES: Correct Value= %04x Actual Value= %04x\n", val, pCPU->es.selector);
		failed= 1;
	}

	val = load_word(fp);

	if (pCPU->sp != val) {
		fprintf(stdout, "SP: Correct Value= %04x Actual Value= %04x\n", val, pCPU->sp);
		failed= 1;
	}

	val = load_word(fp);

	if (pCPU->bp != val) {
		fprintf(stdout, "BP: Correct Value= %04x Actual Value= %04x\n", val, pCPU->bp);
		failed= 1;
	}

	val = load_word(fp);

	if (pCPU->si != val) {
		fprintf(stdout, "SI: Correct Value= %04x Actual Value= %04x\n", val, pCPU->si);
		failed= 1;
	}

	val = load_word(fp);

	if (pCPU->di != val) {
		fprintf(stdout, "DI: Correct Value= %04x Actual Value= %04x\n", val, pCPU->di);
		failed= 1;
	}

	val = load_word(fp);

	if (pCPU->ip != val) {
		fprintf(stdout, "IP: Correct Value= %04x Actual Value= %04x\n", val, pCPU->ip);
		failed= 1;
	}

	val = load_word(fp);

	if (pCPU->flags != val) {
		fprintf(stdout, "FLAGS: Correct Value= %04x (", val);
		print_flags(val);
		fprintf(stdout, ") Actual Value= %04x (", pCPU->flags);
		print_flags(pCPU->flags);
		fprintf(stdout, ")\n");
		//Actual Value = % 04x", val, pCPU->flags);
		failed = 1;
	}


	return failed;
}

int check_mem(FILE* fp) {
	int num = fgetc(fp);
	int i;
	unsigned int addr;
	unsigned char val;
	int failed = 0;

	for (i = 0; i < num; i++) {
		fread(&addr, 4, 1, fp);
		fread(&val, 1, 1, fp);

		if (val != bus_read_8(addr)) {
			printf("%05x should be %02x but is %02x\n", addr, val, bus_read_8(addr));
			failed = 1;
		}
	}

	return failed;
}

int do_386test(FILE* fp) {
	i386 cpu;
	int failed = 0;

	memset(&cpu, 0, sizeof(i386));
	cpu.sgmt_override = DS;

	//skip the size field
	fseek(fp, 1, SEEK_SET);

	//load register context
	load_regs(fp, &cpu);

	//load memory context
	load_mem(fp);

	//step
	i386_step(&cpu);

	//check regs
	if (check_regs(fp, &cpu)) {
		failed |= 1;
	}

	//check mem
	if (check_mem(fp)) {
		failed |= 2;
	}

	//dump_386(&cpu);

	return failed;
}

int _main(int argc, char** argv) {
	FILE* fp = fopen(argv[1], "rb");
	int res;
	Os2Base = malloc(1024 * 1024 + 65280);
	memset(Os2Base, 0, 1024 * 1024 + 65280);

	res = do_386test(fp);
	fclose(fp);

	if (!res) {
		printf("Test completed successfully.\n");
	}

	return res;
}