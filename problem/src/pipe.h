/*
 * CMSC 22200
 *
 * ARM pipeline timing simulator
 */

#ifndef _PIPE_H_
#define _PIPE_H_

#include "shell.h"
#include "bp.h"
#include "stdbool.h"
#include <limits.h>


typedef struct CPU_State {
	/* register file state */
	int64_t REGS[ARM_REGS];
	int FLAG_N;        /* flag N */
	int FLAG_Z;        /* flag Z */

	/* program counter in fetch stage */
	uint64_t PC;

} CPU_State;

int RUN_BIT;

/* global variable -- pipeline state */
extern CPU_State CURRENT_STATE;

extern uint64_t predicted_pc;
extern int btb_miss_temp;
// use enumerated types to make equality tests faster & avoid strings

// define opcodes
enum op{ADD, ADDI, ADDS, ADDSI, CBNZ, CBZ, AND, ANDS, EOR, ORR, LDUR64, LDUR32, LDURB, LDURH, LSLI, LSRI, MOVZ, STUR64, STUR32, STURB, STURH, SUB, SUBI, SUBS, SUBSI, MUL, HLT, CMP, CMPI, BR, B, BEQ, BNE, BGT, BLT, BGE, BLE, BUB};

// define instruction types
// note: use BI for instruction type B to differentiate from opcode B
enum instr_type{R, I, D, BI, CB, IW};

// define a struct that holds the pieces encoded in an instruction type
typedef struct instr_data {
	uint64_t rm;
	uint64_t shamt;
	uint64_t rn;
	uint64_t rd;
	int64_t immediate;
	int64_t address;
	uint64_t op2;
	uint64_t rt;
} instr_data;

// define a struct that holds opcode, instruction type, and instruction data
typedef struct decoded_instr {
	enum op opcode;
	enum instr_type type;
	instr_data data;
} decoded_instr;


typedef struct Pipe_Reg_IFtoDE {
	uint64_t pc;
	uint64_t actually_pc;
	bool bubble;
	int flag_z;
	int flag_n;
	uint64_t predicted_pc;
	int btb_miss;
} Pipe_Reg_IFtoDE;

typedef struct Pipe_Reg_DEtoEX {
	uint64_t pc;
	uint64_t actually_pc;
	instr_data instr_data;
	decoded_instr decoded_instr;
	int64_t res;
	int dep;
	int flag_z;
	int flag_n;
	int branch_cond;
	uint64_t predicted_pc;
	int btb_miss;
} Pipe_Reg_DEtoEX;

typedef struct Pipe_Reg_EXtoMEM {
	uint64_t pc;
	uint64_t actually_pc;
    instr_data instr_data;
    decoded_instr decoded_instr;
    int64_t res;
	int branch_cond;
	int branch_taken;
	int flag_z;
	int flag_n;
	int btb_miss;
} Pipe_Reg_EXtoMEM;

typedef struct Pipe_Reg_MEMtoWB {
	uint64_t pc;
	uint64_t actually_pc;
	instr_data instr_data;
	decoded_instr decoded_instr;
	int64_t res;
	int flag_z;
	int flag_n;
	bool flags_set;
} Pipe_Reg_MEMtoWB;


typedef struct b {
	int bits: 19;
} bit_19;

typedef struct b2 {
	int bits: 26;
} bit_26;


/* called during simulator startup */
void pipe_init();

/* this function calls the others */
void pipe_cycle();

/* each of these functions implements one stage of the pipeline */
void pipe_stage_fetch();
void pipe_stage_decode();
void pipe_stage_execute();
void pipe_stage_mem();
void pipe_stage_wb();

#endif
