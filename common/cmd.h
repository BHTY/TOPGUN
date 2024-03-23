#pragma once

typedef struct{
	int cmd;
	int args[4];
} cmd_pkt;

typedef struct{
	unsigned short cs;
	unsigned long ip;
} brk_resp;

typedef struct{
	brk_resp csip;
	int num_bytes;
	unsigned char bytes[16];
} step_pkt;

//Command Numbers
#define DUMP_REGS 0
#define DUMP_MEMORY 1
#define LOAD_MEMORY 2
#define DUMP_GDT 3
#define DUMP_IDT 4
#define DUMP_LDT 5
#define SET_BP 6
#define UNSET_BP 7
#define SET_WP 8
#define UNSET_WP 9
#define LIST_BP 10
#define LIST_WP 11
#define TRACE 12
#define STEP_OVER 13
#define STEP_OUT 14
#define GO 15
#define BREAK 16