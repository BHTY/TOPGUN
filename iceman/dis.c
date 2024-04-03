#include "../common/i386.h"

char destination[50];
char source1[50];
char source2[50];
char rm_str[50];

char* rm16_strs[] = {"BX+SI", "BX+DI", "BP+SI", "BP+DI", "SI", "DI", "BP", "BX"};

void pdisp16(char* dest_str, int16_t num){
	if(num >= 0){
		sprintf(dest_str, "+0x%04x", num);
	}else{
		sprintf(dest_str, "-0x%04x", -num);
	}
}

void pdisp8(char* dest_str, int8_t num){
	if(num >= 0){
		sprintf(dest_str, "+0x%02x", num);
	}else{
		sprintf(dest_str, "-%0x02x", -num);
	}
}

void pdisp32(char* dest_str, int32_t num){
	if(num >= 0){
		sprintf(dest_str, "+0x%08x", num);
	}else{
		sprintf(dest_str, "-0x%08x", -num);
	}
}

int rm16(unsigned char* address, int op_sz, char mod, char reg, char rm){
	
	int sz = 0;
	char disp[16];
	
	rm_str[0] = 0;
	
	if(mod == 3){
		return sz;
	}
	
	switch(rm){
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 7:
			strcat(rm_str, rm16_strs[rm]);
			break;
		case 6:
			if(mod == 0){ //disp16
				sprintf(rm_str, "0x%04x", *(uint16_t*)(address));
				sz = 2;
			}else{
				strcat(rm_str, rm16_strs[rm]);
			}
			break;
	}
	
	if(mod == 1){
		pdisp8(disp, *(uint8_t*)(address));
		sz = 1;	
		strcat(rm_str, disp);
	} else if(mod == 2){
		pdisp16(disp, *(uint16_t*)(address));
		sz = 2;
		strcat(rm_str, disp);
	}
	
	return sz;
	
}

void print_flag(int flag){
	switch(flag){
		case CARRY:
			printf("C");
			break;
		case INTERRUPT:
			printf("I");
			break;
		case DIRECTION:
			printf("D");
			break;
	}
}

int disasm_operand(char* string, unsigned char* address, i386_operand* op, char reg, char mod, char rm, int op_sz, int addr_sz, int segment_override, int which_segment){
	int actual_size;
	char** reg_table;
	char* sz_str;
	uint16_t offset;

	if(op->type == NONE) return 0;

	actual_size = op->size;

	//convert conditional sized operands to real size
	switch(op->size){
		case SZ_1632: //word / dword
			actual_size = op_sz ? SZ_32 : SZ_16;
			break;
		case SZ_816:
			actual_size = op_sz ? SZ_16 : SZ_8;
			break;
		case SZ_3248: //far pointer (16:16 / 16:32)
			actual_size = op_sz ? SZ_48 : SZ_32;
			break;
		case SZ_3264: //DX:AX / EDX:EAX
			actual_size = op_sz ? SZ_64 : SZ_32;
		default:
			break;
	}

	switch(actual_size){
		case SZ_8:
	    	reg_table = regs_8;
			sz_str = "BYTE";
			break;
		case SZ_16:
			reg_table = regs_16;
			sz_str = "WORD";
			break;
		case SZ_32:
			reg_table = regs_32;
			sz_str = "DWORD";
			break;
		default:
			break;
	}

	switch(op->type){
		case MEM_MODRM:
		case RM_MODRM:
			if(mod == 3){
				strcpy(string, reg_table[rm]);
			}else{
				if(segment_override){
					sprintf(string, "%s PTR %s:[%s]", sz_str, seg_regs[which_segment], rm_str);
				}else{
					sprintf(string, "%s PTR [%s]", sz_str, rm_str);
				}
			}
			break;
		case REG_MODRM:
			strcpy(string, reg_table[reg]);
			break;
		case CREG_MODRM:
			break;
		case SEG_REG_MODRM:
			strcpy(string, seg_regs[reg]);
			break;
		case CONST_REG:
			strcpy(string, reg_table[op->value]);
			break;
		case SIMM:
			break;
		case OFFSET:
			if(addr_sz){
				offset = *(uint32_t*)address;
				if(segment_override){
					sprintf(string, "%s:[0x%08x]", seg_regs[which_segment], offset);
				}else{
					sprintf(string, "[0x%08x]", offset);
				}
			}else{
				offset = *(uint16_t*)address;
				if(segment_override){
					sprintf(string, "%s:[0x%04x]", seg_regs[which_segment], offset);
				}else{
					sprintf(string, "[0x%04x]", offset);
				}
				
				return 2;
			}
		case IMM:

			switch(actual_size){
				case SZ_8:
					sprintf(string, "0x%02x", *address);
					return 1;
				case SZ_16:
					sprintf(string, "0x%04x", *(uint16_t*)address);
					return 2;
				case SZ_32:
					sprintf(string, "0x%08x", *(uint32_t*)address);
					return 4;
				default:
					printf("help\n");
					while(1);
					break;
			}

			break;
		case FAR_PTR:
			break;
		case CONSTANT:
			sprintf(string, "0%02xh", op->value);
			break;
		case IP_REL:
			break;
		default:
			break;
	}

	return 0;
}

int dis386(unsigned char* address, int vaddr, int op_sz, int addr_sz, int segment_override, int which_segment){
	uint8_t byte = *(address++);
	i386_op inst = InstructionTable[byte];
	int size = 1;
	char modrm, reg, rm, mod;
	int alu_op;

	//prefixes
	switch(inst.op_type){
		case LOCK:
			printf("LOCK ");
			return dis386(address, vaddr + 1, op_sz, addr_sz, segment_override, which_segment) + 1;
		case OP_SZ_OVERRIDE:
			return dis386(address, vaddr + 1, !op_sz, addr_sz, segment_override, which_segment) + 1;
		case ADDR_SZ_OVERRIDE:
			return dis386(address, vaddr + 1, op_sz, !addr_sz, segment_override, which_segment) + 1;
		case SEGMENT_OVERRIDE:
			return dis386(address, vaddr + 1, op_sz, !addr_sz, 1, inst.sub_type) + 1;
		case EXTENDED:
			byte = *(address++);
			inst = ExtendedInstructionTable[byte];
			size++;
			break;
		case REPEZ:
			printf("REPE ");
			return dis386(address, vaddr + 1, op_sz, addr_sz, segment_override, which_segment) + 1;
		case REPNE:
			printf("REPNE ");
			return dis386(address, vaddr + 1, op_sz, addr_sz, segment_override, which_segment) + 1;
		default:
			break;
	}

	if(inst.flags & USE_MODRM){
		modrm = *(address++);
		size++;
		reg = REG(modrm);
		rm = RM(modrm);
		mod = MOD(modrm);

		if(addr_sz){
			//size += rm32...
		}else{
			size += rm16(address, op_sz, mod, reg, rm);
		}
	}

	//decode operands
	size += disasm_operand(destination, address, &(inst.dest), reg, mod, rm, op_sz, addr_sz, segment_override, which_segment);
	size += disasm_operand(source1, address, &(inst.src1), reg, mod, rm, op_sz, addr_sz, segment_override, which_segment);
	size += disasm_operand(source2, address, &(inst.src2), reg, mod, rm, op_sz, addr_sz, segment_override, which_segment);

	//disassemble instructions
	switch(inst.op_type){
		case ALU:
			alu_op = (inst.sub_type == ALU_MULTI) ? reg : inst.sub_type;
			printf("%s %s, %s\n", alu_op_names[alu_op], source1, source2);
			break;
		case XCHG:
			printf("XCHG %s, %s\n", source1, source2);
			break;
		case MOV_TO_SR:
		case MOV:
			printf("MOV %s, %s\n", destination, source1);
			break;
		case INT_OP:
			printf("INT %s\n", source1);
			break;
		case CLR_FLAG:
			printf("CL");
			print_flag(inst.sub_type);
			printf("\n");
			break;
		case ST_FLAG:
			printf("ST");
			print_flag(inst.sub_type);
			printf("\n");
			break;
		default:
			printf("unimplemented op %02x (operation %02x)\n", byte, inst.op_type);
			break;
	}

	return size;
}
