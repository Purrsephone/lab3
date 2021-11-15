/*
 * CMSC 22200
 *
 * ARM pipeline timing simulator
 */

#include "pipe.h"
#include "shell.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <inttypes.h>


// Bhakti Shah, cnet: bhaktishah
// Sophie Veys, cnet: sophiev
//void init_to_null_MEM_to_WB();
//void init_to_null_EX_to_MEM();
uint64_t pc;
uint64_t hlt_pc;
uint64_t pc_for_real;
uint64_t predicted_pc;
uint64_t tmp_pc_reset;

int cycles = 0;
int stall = 0;
int flush = 0;
int make_bub = 0;
int resetting_pc = 0;
int manually_set_pc = 0;
int was_reset = 0;
int btb_miss_temp = 0;

Pipe_Reg_IFtoDE IF_to_DE = {
    .pc = 0,
    .actually_pc = 0,
    .bubble = false,
    .predicted_pc = 0
};

Pipe_Reg_DEtoEX DE_to_EX = {
	.pc = 0,
    .actually_pc = 0,
	.instr_data = {
        .rm = 0,
    	.shamt = 0,
    	.rn = 0,
    	.rd = 0,
    	.immediate = 0,
    	.address = 0,
    	.op2 = 0,
    	.rt = 0
    },
	.decoded_instr = {
        .opcode = 0,
    	.type = 0
    },
    .res = 0,
    .branch_cond = 0,
    .dep = 0,
    .flag_z = 0,
    .flag_n = 0,
    .predicted_pc = 0
};

Pipe_Reg_EXtoMEM EX_to_MEM = {
	.pc = 0,
    .actually_pc = 0,
	.instr_data = {
        .rm = 0,
    	.shamt = 0,
    	.rn = 0,
    	.rd = 0,
    	.immediate = 0,
    	.address = 0,
    	.op2 = 0,
    	.rt = 0
    },
	.decoded_instr = {
        .opcode = 0,
    	.type = 0
    },
    .res = 0,
    .branch_cond = 0,
    //0 if not checked, 1 if taken, 2 if not taken
    .branch_taken = 0,
    .flag_z = 0,
    .flag_n = 0
};

Pipe_Reg_MEMtoWB MEM_to_WB = {
	.pc = 0,
    .actually_pc = 0,
	.instr_data = {
        .rm = 0,
    	.shamt = 0,
    	.rn = 0,
    	.rd = 0,
    	.immediate = 0,
    	.address = 0,
    	.op2 = 0,
    	.rt = 0
    },
	.decoded_instr = {
        .opcode = 0,
    	.type = 0
    },
    .flag_z = 0,
    .flag_n = 0,
    .flags_set = false
};

/* global pipeline state */
CPU_State CURRENT_STATE;
// 0 if no branch, 1 if branch
int branch_exists = 0;
    //0 if not checked, 1 if taken, 2 if not taken
int branch_taken = 0;
uint64_t branch_pc = 0;
void pipe_init()
{
    memset(&CURRENT_STATE, 0, sizeof(CPU_State));
    CURRENT_STATE.PC = 0x00400000;
    predicted_pc = CURRENT_STATE.PC;
}

bool call_wb = true, call_mem = true, call_ex = true, call_id = true, call_if = true;
uint64_t branch_ins_add = 0;
void pipe_cycle() {
    printf("\n CYCLE %i\n", (cycles + 1));
    if (call_wb) {
        pipe_stage_wb();
        //printf("wb flags z %i n %i\n", MEM_to_WB.flag_z, MEM_to_WB.flag_n);
    }
    if (call_mem) {
        pipe_stage_mem();
        //printf("mem flags z %i n %i\n", EX_to_MEM.flag_z, EX_to_MEM.flag_n);
    }
    if (call_ex) {
        pipe_stage_execute();
       // printf("execute flags z %i n %i\n", DE_to_EX.flag_z, DE_to_EX.flag_n);
    }
    if (stall == 0) {
        printf("stall is 0\n");
        if (call_id) {
        printf("call_id is 0\n");
        pipe_stage_decode();
       // printf("decode flags z %i n %i\n", IF_to_DE.flag_z, IF_to_DE.flag_n);
        }
        if (call_if) {
            printf("call if is set\n");
            pipe_stage_fetch();
           // printf("current_state flags z %i n %i\n", CURRENT_STATE.FLAG_Z, CURRENT_STATE.FLAG_N);

        }
        if (call_id) {
            branch_exists = DE_to_EX.branch_cond;
        }
        if (call_ex) {
            branch_taken = EX_to_MEM.branch_taken;
        }
        call_wb =  call_mem;
        call_mem = call_ex;
        call_ex = call_id;
        //call_id = !branch_exists;

        if (branch_exists) {
            printf("branch exists\n");
            CURRENT_STATE.PC += 4;
            if (branch_taken == 0) {
                //call_if = false;
            }
        }
        else {
            //printf("decode definitely does not have branch / execute may have branch\n");
            if (branch_taken != 0) {
                if (branch_taken == 1) {
                    //printf("branch taken %lx\n", branch_pc);
                    if (branch_pc == (DE_to_EX.actually_pc + 4)) {
                       // printf("branch target same as next\n");
                        call_id = true;
                        //CURRENT_STATE.PC = CURRENT_STATE.PC + 4;
                        //printf("CURRENT_STATE %lx\n", CURRENT_STATE.PC);
                        call_if = true;
                    }
                    else {
                        //printf("branch target not same as next\n");
                        CURRENT_STATE.PC = branch_pc;
                        //call_id = false;
                        call_if = true;
                    }
                }
                if (branch_taken == 2) {
                   // printf("branch not taken\n");
                    //CURRENT_STATE.PC = CURRENT_STATE.PC + 4;
                    call_id = true;
                    call_if = true;
                }
            }
            else {
                // TODO: SEE IF RIGHT 
               // printf("HERE");
                CURRENT_STATE.PC += 4;
                call_id = true;
                call_if = true;
            }
        }
    }
    else {
        stall = 0;
    }
    branch_exists = 0;
    branch_taken = 0;
    CURRENT_STATE.REGS[31] = 0;
    if(manually_set_pc == 1) {
            printf("PC IS BEING MANUALLY SET\n");
            CURRENT_STATE.PC = branch_pc;
            manually_set_pc = 0;

    }
    else if(was_reset == 1) {
        CURRENT_STATE.PC = tmp_pc_reset + 4;
        was_reset = 0;
    }
    else {
        printf("PC IS BEING PREDICTED\n");
        CURRENT_STATE.PC = predicted_pc;
        printf("PREDICTED PC: %lx\n", CURRENT_STATE.PC);
    }
    cycles++;
    
    // let's print the btb 
    /*
    for(int i = 0; i < 81; i++) {
        printf("PHT %i: %u\n", i, bp.gshare.pht[i]);
    }
    
   for (int j = 0; j < 81; j++) {
       printf("tag %li\n", bp.btb.entries[j].tag);
        printf("target %li\n", bp.btb.entries[j].target);
   }
   */
    printf("GHR IS: %i\n", bp.gshare.ghr);
    printf("val of btb miss in IF is: %i\n", IF_to_DE.btb_miss);
    printf("val of btb miss in DE is: %i\n", DE_to_EX.btb_miss);
    printf("val of btb miss in EX is: %i\n", EX_to_MEM.btb_miss);
}

//DE_to_EX.res will be the value that we grabbed
//DE_to_EX.dep will tell us which value (if any), to replace
// .dep = 0 (no dependency), 1 (rm), 2 (rn)
void check_dependency()
{
    if(cycles >= 3)
    {

        // STALL condition
        if((MEM_to_WB.decoded_instr.opcode == LDUR64) ||
        (MEM_to_WB.decoded_instr.opcode == LDUR32) ||
        (MEM_to_WB.decoded_instr.opcode == LDURB) ||
        (MEM_to_WB.decoded_instr.opcode == LDURH)) {
            //printf("opcode de to ex %i\n",DE_to_EX.decoded_instr.opcode);
            //printf("opcode ex to mem %i\n",EX_to_MEM.decoded_instr.opcode);
            //printf("rt LDUR %li\n", MEM_to_WB.instr_data.rt);
            //printf("rt STUR (de to ex) %li\n", DE_to_EX.instr_data.rt);
            //printf("rt STUR (ex to mem) %li\n", EX_to_MEM.instr_data.rt);
            if(EX_to_MEM.instr_data.rm == MEM_to_WB.instr_data.rt) {
                stall = 1;
                return;
            }
            if(EX_to_MEM.instr_data.rn == MEM_to_WB.instr_data.rt) {
                stall = 1;
                return;
            }
            if((EX_to_MEM.instr_data.rt == MEM_to_WB.instr_data.rt) &&
                ((EX_to_MEM.decoded_instr.opcode == STUR64) ||
                (EX_to_MEM.decoded_instr.opcode == STUR32) ||
                (EX_to_MEM.decoded_instr.opcode == STURB) ||
                (EX_to_MEM.decoded_instr.opcode == STURH))) {
                    stall = 1;
		            //printf("HERE\n");
                    return;
                }
        }
        //check if instr dependent on another instr in memory stage

        //check if rm dependent
	//printf("EX to ME rm  %lu\n", EX_to_MEM.instr_data.rm);
	//printf("EX to ME rn  %lu\n", EX_to_MEM.instr_data.rn);
	//printf("MEM to WB rd  %lu\n", MEM_to_WB.instr_data.rd);
	//printf("MEM to WB rt  %lu\n", MEM_to_WB.instr_data.rt);
        else if(EX_to_MEM.instr_data.rm == MEM_to_WB.instr_data.rd) {
            DE_to_EX.res = MEM_to_WB.res;
            DE_to_EX.dep = 1;
	           //printf("oops\n");
        }
        //check if rn dependent
        else if(EX_to_MEM.instr_data.rn == MEM_to_WB.instr_data.rd) {
                DE_to_EX.res = MEM_to_WB.res;
                DE_to_EX.dep = 2;
	            //printf("oops 2\n");
        }

        // check if instr dependent on LDUR in mem stage
        /* ACTUALLY I THINK WE NEED TO STALL
        else if((MEM_to_WB.decoded_instr.opcode == LDUR64) ||
        (MEM_to_WB.decoded_instr.opcode == LDUR32) ||
        (MEM_to_WB.decoded_instr.opcode == LDURB) ||
        (MEM_to_WB.decoded_instr.opcode == LDURH)) {
            if(EX_to_MEM.instr_data.rm == MEM_to_WB.instr_data.rt) {
                DE_to_EX.res = MEM_to_WB.res;
                DE_to_EX.dep = 1;
            }
            if(EX_to_MEM.instr_data.rn == MEM_to_WB.instr_data.rt) {
                DE_to_EX.res = MEM_to_WB.res;
                DE_to_EX.dep = 2;
            }
        }
        */

        //check if rt dependent
        //if instr is stur, which reads from rt reg
        //ACTUALLY, idt we care about STUR
        /*
        if((DE_to_EX.decoded_instr.opcode == STUR64) ||
        (DE_to_EX.decoded_instr.opcode == STUR32) ||
        (DE_to_EX.decoded_instr.opcode == STURB) ||
        (DE_to_EX.decoded_instr.opcode == STURH)) {
            if(DE_to_EX.rt == EX_to_MEM.rd) {
                DE_to_EX.res = EX_to_MEM.res;
                DE_to_EX.dep = 3;
            }
        }
        */
        else {
            DE_to_EX.dep = 0;
        }
        //printf("DEBUG check depen\n");
        //printf("MEM_to_WB.res %li\n", MEM_to_WB.res);
        //printf("DE_to_EX.dep %i\n", DE_to_EX.dep);
        //check  if instr dependent on another instr in memory stage
    }
    CURRENT_STATE.REGS[31] = 0;
}

void pipe_stage_wb()
{
    //printf("writeback %x pc %lx\n", MEM_to_WB.decoded_instr.opcode, MEM_to_WB.actually_pc);
    //printf("mem to wb: %li \n", MEM_to_WB.res);
    // CBNZ, CBZ, STUR64, STUR32, STURB, STURH, HLT
    // BR, B, BEQ, BNE, BGT, BLT, BGE, BLE skip writeback

    if ((MEM_to_WB.decoded_instr.opcode == ADD) ||
        (MEM_to_WB.decoded_instr.opcode == ADDI) ||
        (MEM_to_WB.decoded_instr.opcode == SUB) ||
        (MEM_to_WB.decoded_instr.opcode == SUBI) ||
        (MEM_to_WB.decoded_instr.opcode == MUL) ||
        (MEM_to_WB.decoded_instr.opcode == AND) ||
        (MEM_to_WB.decoded_instr.opcode == EOR) ||
        (MEM_to_WB.decoded_instr.opcode == ORR) ||
        (MEM_to_WB.decoded_instr.opcode == LSLI) ||
        (MEM_to_WB.decoded_instr.opcode == LSRI) ||
        (MEM_to_WB.decoded_instr.opcode == MOVZ) ||
        (MEM_to_WB.decoded_instr.opcode == BUB)) {
        // write to reg
        CURRENT_STATE.REGS[MEM_to_WB.instr_data.rd] = MEM_to_WB.res;
    }
    if (MEM_to_WB.decoded_instr.opcode == ADDS) {
        // write to reg
        CURRENT_STATE.REGS[MEM_to_WB.instr_data.rd] = MEM_to_WB.res;
        // update flags
    	if(MEM_to_WB.res == 0) {
    		CURRENT_STATE.FLAG_Z = 1;
    	}
    	if(MEM_to_WB.res < 0) {
    		CURRENT_STATE.FLAG_N = 1;
    	}

    }
    if (MEM_to_WB.decoded_instr.opcode == ADDSI) {
        // write to reg
	    CURRENT_STATE.REGS[MEM_to_WB.instr_data.rd] = MEM_to_WB.res;
        // update flags
    	if (MEM_to_WB.res == 0) {
    	    CURRENT_STATE.FLAG_Z = 1;
    		CURRENT_STATE.FLAG_N = 0;
    	}
    	if (MEM_to_WB.res < 0) {
        	CURRENT_STATE.FLAG_N = 1;
        	CURRENT_STATE.FLAG_Z = 0;
    	}
    	if (MEM_to_WB.res > 0) {
            CURRENT_STATE.FLAG_N = 0;
            CURRENT_STATE.FLAG_Z = 0;
    	}

    }
    if (MEM_to_WB.decoded_instr.opcode == ANDS) {
        // write to reg
        CURRENT_STATE.REGS[MEM_to_WB.instr_data.rd] = MEM_to_WB.res;
        // update flags
        if (MEM_to_WB.res == 0) {
            CURRENT_STATE.FLAG_Z = 1;
        }
        if (MEM_to_WB.res < 0) {
            CURRENT_STATE.FLAG_N = 1;
        }

    }
    // if a load instruction
    if ((MEM_to_WB.decoded_instr.opcode == LDUR64) ||
        (MEM_to_WB.decoded_instr.opcode == LDUR32) ||
        (MEM_to_WB.decoded_instr.opcode == LDURB) ||
        (MEM_to_WB.decoded_instr.opcode == LDURH)) {
        // write result from memory to register
	CURRENT_STATE.REGS[MEM_to_WB.instr_data.rt] = MEM_to_WB.res;

    }
    if (MEM_to_WB.decoded_instr.opcode == SUBS) {
        if (MEM_to_WB.instr_data.rd != 31) {
            CURRENT_STATE.REGS[MEM_to_WB.instr_data.rd] = MEM_to_WB.res;
        }
        if (MEM_to_WB.res == 0) {
            CURRENT_STATE.FLAG_Z = 1;
            CURRENT_STATE.FLAG_N = 0;
        }
        if (MEM_to_WB.res < 0) {
            CURRENT_STATE.FLAG_N = 1;
            CURRENT_STATE.FLAG_Z = 0;
        }
        if (MEM_to_WB.res > 0) {
            CURRENT_STATE.FLAG_N = 0;
            CURRENT_STATE.FLAG_Z = 0;
        }
    }
    if (MEM_to_WB.decoded_instr.opcode == SUBSI) {
        CURRENT_STATE.REGS[MEM_to_WB.instr_data.rd] = MEM_to_WB.res;
        if (MEM_to_WB.res == 0) {
            CURRENT_STATE.FLAG_Z = 1;
            CURRENT_STATE.FLAG_N = 0;
        }
        if (MEM_to_WB.res < 0) {
            CURRENT_STATE.FLAG_N = 1;
            CURRENT_STATE.FLAG_Z = 0;
        }
        if (MEM_to_WB.res > 0) {
            CURRENT_STATE.FLAG_N = 0;
            CURRENT_STATE.FLAG_Z = 0;
        }


    }
    if (MEM_to_WB.decoded_instr.opcode == HLT) {
	//     printf("meow!!");
	     CURRENT_STATE.PC = hlt_pc;
	     //stat_cycles++;
	     stat_inst_retire++;
             RUN_BIT = 0;
	     return;
      }
	if((cycles > 3) && (MEM_to_WB.decoded_instr.opcode != BUB))
	{
        //printf("retiring instr %lx %x\n", MEM_to_WB.actually_pc, MEM_to_WB.decoded_instr.opcode);
		stat_inst_retire++;
	}
    else {
        //printf("not retiring\n");
    }
    MEM_to_WB.decoded_instr.opcode = 0;
}

void pipe_stage_mem()
{
    if (EX_to_MEM.decoded_instr.opcode == ADDS ||
    EX_to_MEM.decoded_instr.opcode == ADDSI ||
    EX_to_MEM.decoded_instr.opcode == SUBS ||
    EX_to_MEM.decoded_instr.opcode == SUBSI ||
    EX_to_MEM.decoded_instr.opcode == ANDS ||
    EX_to_MEM.decoded_instr.opcode == CMP ||
    EX_to_MEM.decoded_instr.opcode == CMPI) {
        MEM_to_WB.flags_set = true;
    }
    else {
        MEM_to_WB.flags_set = false;
    }
    MEM_to_WB.actually_pc = EX_to_MEM.actually_pc;
    MEM_to_WB.flag_n = EX_to_MEM.flag_n;
    MEM_to_WB.flag_z = EX_to_MEM.flag_z;
    // if (EX_to_MEM.pc == 0) {
    //     printf("EX_to_MEM.pc = 0\n");
    //     return;
    // }
   //printf("%u\n", EX_to_MEM.decoded_instr.opcode);
    //printf("memory %x pc %lx\n", EX_to_MEM.decoded_instr.opcode, EX_to_MEM.actually_pc);
   // printf("stat cycle: %d\n", stat_cycles);
    if(stall == 0) {

    //pass along info from one stage to next
	MEM_to_WB.decoded_instr = EX_to_MEM.decoded_instr;
    MEM_to_WB.instr_data = EX_to_MEM.instr_data;
    MEM_to_WB.res = EX_to_MEM.res;
    //printf("MEM TO WB res %li\n", MEM_to_WB.res);
    // ADD, ADDI, ADDS, ADDSI, CBNZ, CBZ, AND, ANDS, EOR, ORR
    // LSLI, LSRI, MOVZ, SUB, SUBI, SUBS, SUBSI, MUL, CMP, CMPI
    // BR, B, BEQ, BNE, BGT, BLT, BGE, BLE can SKIP mem stage

    // LDUR64, LDUR32, LDURB, LDURH, STUR64, STUR32, STURB, STURH

    // when HLT is in mem stage, we want to stop running the program
     if (EX_to_MEM.decoded_instr.opcode == LDUR64) {
         // go into memory
	     uint32_t first_32 = mem_read_32(EX_to_MEM.res);
         uint32_t second_32 = mem_read_32(EX_to_MEM.res + 4);
         int64_t doubleword = ((uint64_t)second_32 << 32) | first_32;
         // pass result into writeback
	     MEM_to_WB.res = doubleword;
     }
     if (EX_to_MEM.decoded_instr.opcode == LDUR32) {
        // go into memory
        // pass result into writeback
        MEM_to_WB.res = mem_read_32(EX_to_MEM.res);
     }
     if (EX_to_MEM.decoded_instr.opcode == LDURB) {
         // go into memory
         // pass result into writeback
         MEM_to_WB.res = (mem_read_32(EX_to_MEM.res) & 0xFF);
     }
     if (EX_to_MEM.decoded_instr.opcode == LDURH) {
         // go into memory
         // pass result into writeback
         MEM_to_WB.res = (mem_read_32(EX_to_MEM.res) & 0xFFFF);
     }
     if (EX_to_MEM.decoded_instr.opcode == STUR64) {
	     //printf("stur64");
         uint32_t first_32 = (uint32_t)((CURRENT_STATE.REGS[EX_to_MEM.instr_data.rt] & 0xFFFFFFFF00000000LL) >> 32);
         uint32_t second_32 = (uint32_t)(CURRENT_STATE.REGS[EX_to_MEM.instr_data.rt] & 0xFFFFFFFFLL);
         // write result into memory
         mem_write_32(EX_to_MEM.res, second_32);
         mem_write_32((EX_to_MEM.res + 4), first_32);
     }
     if (EX_to_MEM.decoded_instr.opcode == STUR32) {
         // write result into memory
	 //printf("stur32");
         mem_write_32(EX_to_MEM.res, CURRENT_STATE.REGS[EX_to_MEM.instr_data.rt]);
     }
     if (EX_to_MEM.decoded_instr.opcode == STURB) {
         // write result into memory
	 // printf("sturb\n");
	 //printf("rt %li\n", CURRENT_STATE.REGS[EX_to_MEM.instr_data.rt] & 0xFF);
         mem_write_32(EX_to_MEM.res, (CURRENT_STATE.REGS[EX_to_MEM.instr_data.rt] & 0xFF));
     }
     if (EX_to_MEM.decoded_instr.opcode == STURH) {
         // write result into memory
	 //printf("sturh");
         mem_write_32(EX_to_MEM.res, (CURRENT_STATE.REGS[EX_to_MEM.instr_data.rt] & 0xFFFF));
     }
     if (EX_to_MEM.decoded_instr.opcode == HLT) {
	     CURRENT_STATE.PC = hlt_pc;
     }
    }
    EX_to_MEM.decoded_instr.opcode = 0;
}

void pipe_stage_execute()
{
    printf("opcode %i \n", DE_to_EX.decoded_instr.opcode);
    if (DE_to_EX.actually_pc == 0) {
        EX_to_MEM.actually_pc = 0;
        return;
    }
    //printf("execute %x pc %lx\n", DE_to_EX.decoded_instr.opcode, DE_to_EX.actually_pc);
    EX_to_MEM.branch_cond = 0;
    EX_to_MEM.branch_taken = 0;
    // check dependency
    if (DE_to_EX.decoded_instr.opcode == BUB) {
        EX_to_MEM.decoded_instr.opcode = BUB;
        EX_to_MEM.res = 0;
        EX_to_MEM.instr_data.rd = 31;
        return;
    }
    else {
        EX_to_MEM.actually_pc = DE_to_EX.actually_pc;
        EX_to_MEM.decoded_instr = DE_to_EX.decoded_instr;
        EX_to_MEM.instr_data = DE_to_EX.instr_data;
    }
    // pass along info from one stage to next
    EX_to_MEM.flag_n = DE_to_EX.flag_n;
    EX_to_MEM.flag_z = DE_to_EX.flag_z;
    EX_to_MEM.btb_miss = DE_to_EX.btb_miss;
    
	check_dependency();

    //do a nonsense instr, like set 0 reg = 0
    if(stall == 1) {
        EX_to_MEM.decoded_instr.opcode = BUB;
        EX_to_MEM.res = 0;
        EX_to_MEM.instr_data.rd = 31;
        return;
    }

    if(stall == 0) {
        // do addition
        if (DE_to_EX.decoded_instr.opcode == ADDS) {
        // no dependency
            if(DE_to_EX.dep == 0) {
                EX_to_MEM.res = CURRENT_STATE.REGS[DE_to_EX.instr_data.rm]
                + CURRENT_STATE.REGS[DE_to_EX.instr_data.rn];
            }
            // replace rm with forwarded value
            if(DE_to_EX.dep == 1) {
                EX_to_MEM.res = DE_to_EX.res + CURRENT_STATE.REGS[DE_to_EX.instr_data.rn];
            }
            // replace rn with forwarded value
            if(DE_to_EX.dep == 2) {
                EX_to_MEM.res = DE_to_EX.res + CURRENT_STATE.REGS[DE_to_EX.instr_data.rm];
            }
            if (EX_to_MEM.res == 0) {
                EX_to_MEM.flag_z = 1;
                EX_to_MEM.flag_n = 0;
            }
            if (EX_to_MEM.res < 0) {
                EX_to_MEM.flag_n = 1;
                EX_to_MEM.flag_z = 0;
            }
            if (EX_to_MEM.res > 0) {
                EX_to_MEM.flag_n = 0;
                EX_to_MEM.flag_z = 0;
            }
        }
        if (DE_to_EX.decoded_instr.opcode == ADD) {
            // no dependency
            if(DE_to_EX.dep == 0) {
                EX_to_MEM.res = CURRENT_STATE.REGS[DE_to_EX.instr_data.rm]
                + CURRENT_STATE.REGS[DE_to_EX.instr_data.rn];
            }
            // replace rm with forwarded value
            if(DE_to_EX.dep == 1) {
                EX_to_MEM.res = DE_to_EX.res + CURRENT_STATE.REGS[DE_to_EX.instr_data.rn];
            }
            // replace rn with forwarded value
            if(DE_to_EX.dep == 2) {
                EX_to_MEM.res = DE_to_EX.res + CURRENT_STATE.REGS[DE_to_EX.instr_data.rm];
            }
        }
        if (DE_to_EX.decoded_instr.opcode == ADDSI) {
            // no dependency
            if((DE_to_EX.dep == 0) || (DE_to_EX.dep == 1)) {
                    EX_to_MEM.res = CURRENT_STATE.REGS[DE_to_EX.instr_data.rn] + DE_to_EX.instr_data.immediate;
            }
            // replace rn with forwarded value
            if(DE_to_EX.dep == 2) {
                EX_to_MEM.res = DE_to_EX.res + DE_to_EX.instr_data.immediate;
            }
            if (EX_to_MEM.res == 0) {
                EX_to_MEM.flag_z = 1;
                EX_to_MEM.flag_n = 0;
            }
            if (EX_to_MEM.res < 0) {
                EX_to_MEM.flag_n = 1;
                EX_to_MEM.flag_z = 0;
            }
            if (EX_to_MEM.res > 0) {
                EX_to_MEM.flag_n = 0;
                EX_to_MEM.flag_z = 0;
            }
        }
        if (DE_to_EX.decoded_instr.opcode == ADDI) {
            // no dependency
            if((DE_to_EX.dep == 0) || (DE_to_EX.dep == 1)) {
                    EX_to_MEM.res = CURRENT_STATE.REGS[DE_to_EX.instr_data.rn] + DE_to_EX.instr_data.immediate;
            }
            // replace rn with forwarded value
            if(DE_to_EX.dep == 2) {
                EX_to_MEM.res = DE_to_EX.res + DE_to_EX.instr_data.immediate;
            }
        }
        // do branching
        if (DE_to_EX.decoded_instr.opcode == CBNZ) {
            EX_to_MEM.branch_cond = 1;
            if (CURRENT_STATE.REGS[DE_to_EX.instr_data.rt]) {
                EX_to_MEM.branch_taken = 1;
                branch_pc = ((int64_t) DE_to_EX.actually_pc)
                    + DE_to_EX.instr_data.address;
            }
            else {
                EX_to_MEM.branch_taken = 2;
            }
        }
        if (DE_to_EX.decoded_instr.opcode == CBZ) {
            EX_to_MEM.branch_cond = 1;
            if (!CURRENT_STATE.REGS[DE_to_EX.instr_data.rt]) {
                EX_to_MEM.branch_taken = 1;
                branch_pc = ((int64_t) DE_to_EX.actually_pc)
                    + DE_to_EX.instr_data.address;
            }
            else {
                EX_to_MEM.branch_taken = 2;
            }
        }
        if (DE_to_EX.decoded_instr.opcode == BR) {
            EX_to_MEM.branch_cond = 1;
            EX_to_MEM.branch_taken = 1;
            branch_pc = CURRENT_STATE.REGS[DE_to_EX.instr_data.rn];
        }
        if (DE_to_EX.decoded_instr.opcode == B) {
            //printf("DE_TO_EX.instr_data.address %lu + DE_to_EX pc %lx\n", DE_to_EX.instr_data.address, DE_to_EX.actually_pc);
            branch_pc = (uint64_t) DE_to_EX.actually_pc + DE_to_EX.instr_data.address;
            EX_to_MEM.branch_taken = 1;
            EX_to_MEM.branch_cond = 1;

        }
        int zflag = MEM_to_WB.flags_set ? MEM_to_WB.flag_z : CURRENT_STATE.FLAG_Z;
        int nflag = MEM_to_WB.flags_set ? MEM_to_WB.flag_n : CURRENT_STATE.FLAG_N;
        if (DE_to_EX.decoded_instr.opcode == BEQ) {
            EX_to_MEM.branch_cond = 2;
            if (zflag) {
                branch_pc = (int64_t) (DE_to_EX.actually_pc) + (DE_to_EX.instr_data.address);
                EX_to_MEM.branch_taken = 1;
            }
            else {
                EX_to_MEM.branch_taken = 2;

            }
        }
        if (DE_to_EX.decoded_instr.opcode == BNE) {
            EX_to_MEM.branch_cond = 2;
            if (!zflag) {
                branch_pc = (int64_t) (DE_to_EX.actually_pc) + (DE_to_EX.instr_data.address);
                EX_to_MEM.branch_taken = 1;
            }
            else {
                EX_to_MEM.branch_taken = 2;
            }
        }
        if (DE_to_EX.decoded_instr.opcode == BGT) {
            EX_to_MEM.branch_cond = 2;
            if ((!zflag) && (!nflag)) {
                branch_pc = (int64_t) (DE_to_EX.actually_pc) + (DE_to_EX.instr_data.address);
                EX_to_MEM.branch_taken = 1;
            }
            else {
                EX_to_MEM.branch_taken = 2;
            }
        }
        if (DE_to_EX.decoded_instr.opcode == BLT) {
            EX_to_MEM.branch_cond = 2;
            if ((nflag && !zflag)) {
                branch_pc = DE_to_EX.actually_pc + DE_to_EX.instr_data.address;
                EX_to_MEM.branch_taken = 1;
            }
            else {
                EX_to_MEM.branch_taken = 2;
            }
        }
        if (DE_to_EX.decoded_instr.opcode == BGE) {
            EX_to_MEM.branch_cond = 2;
            if ((!nflag || zflag)) {
                branch_pc = DE_to_EX.actually_pc + DE_to_EX.instr_data.address;
                EX_to_MEM.branch_taken = 1;
            }
            else {
                EX_to_MEM.branch_taken = 2;
            }
        }
        if (DE_to_EX.decoded_instr.opcode == BLE) {
            EX_to_MEM.branch_cond = 2;
            if ((zflag || nflag)) {
                branch_pc = DE_to_EX.actually_pc + DE_to_EX.instr_data.address;
                EX_to_MEM.branch_taken = 1;
            }
            else {
                EX_to_MEM.branch_taken = 2;
            }
        }
        if (DE_to_EX.decoded_instr.opcode == AND) {
            if(DE_to_EX.dep == 0) {
                EX_to_MEM.res = CURRENT_STATE.REGS[DE_to_EX.instr_data.rm]
                & CURRENT_STATE.REGS[DE_to_EX.instr_data.rn];
            }
            if(DE_to_EX.dep == 1) {
                EX_to_MEM.res = DE_to_EX.res & CURRENT_STATE.REGS[DE_to_EX.instr_data.rn];
            }
            if(DE_to_EX.dep == 2) {
                EX_to_MEM.res = DE_to_EX.res & CURRENT_STATE.REGS[DE_to_EX.instr_data.rm];
            }
        }
        if (DE_to_EX.decoded_instr.opcode == ANDS) {
            if(DE_to_EX.dep == 0) {
                EX_to_MEM.res = CURRENT_STATE.REGS[DE_to_EX.instr_data.rm]
                & CURRENT_STATE.REGS[DE_to_EX.instr_data.rn];
            }
            if(DE_to_EX.dep == 1) {
                EX_to_MEM.res = DE_to_EX.res & CURRENT_STATE.REGS[DE_to_EX.instr_data.rn];
            }
            if(DE_to_EX.dep == 2) {
                EX_to_MEM.res = DE_to_EX.res & CURRENT_STATE.REGS[DE_to_EX.instr_data.rm];
            }
            if (EX_to_MEM.res == 0) {
                EX_to_MEM.flag_z = 1;
                EX_to_MEM.flag_n = 0;
            }
            if (EX_to_MEM.res < 0) {
                EX_to_MEM.flag_n = 1;
                EX_to_MEM.flag_z = 0;
            }
            if (EX_to_MEM.res > 0) {
                EX_to_MEM.flag_n = 0;
                EX_to_MEM.flag_z = 0;
            }
        }
        if (DE_to_EX.decoded_instr.opcode == EOR) {
            if(DE_to_EX.dep == 0) {
                EX_to_MEM.res = CURRENT_STATE.REGS[DE_to_EX.instr_data.rm]
                ^ CURRENT_STATE.REGS[DE_to_EX.instr_data.rn];
            }
            if(DE_to_EX.dep == 1) {
                EX_to_MEM.res = DE_to_EX.res ^ CURRENT_STATE.REGS[DE_to_EX.instr_data.rn];
            }
            if(DE_to_EX.dep == 2) {
                EX_to_MEM.res = DE_to_EX.res ^ CURRENT_STATE.REGS[DE_to_EX.instr_data.rm];
            }
        }
        if (DE_to_EX.decoded_instr.opcode == ORR) {
            if(DE_to_EX.dep == 0) {
                EX_to_MEM.res = CURRENT_STATE.REGS[DE_to_EX.instr_data.rm]
                | CURRENT_STATE.REGS[DE_to_EX.instr_data.rn];
            }
            if(DE_to_EX.dep == 1) {
                EX_to_MEM.res = DE_to_EX.res | CURRENT_STATE.REGS[DE_to_EX.instr_data.rn];
            }
            if(DE_to_EX.dep == 2) {
                EX_to_MEM.res = DE_to_EX.res | CURRENT_STATE.REGS[DE_to_EX.instr_data.rm];
            }
        }

    	if ((DE_to_EX.decoded_instr.opcode == LDUR64) ||
            (DE_to_EX.decoded_instr.opcode == LDUR32)) {
                uint64_t final_addr;
                if ((DE_to_EX.dep == 0) || (DE_to_EX.dep == 1)) {
                     final_addr = (DE_to_EX.instr_data.address + CURRENT_STATE.REGS[DE_to_EX.instr_data.rn]);
                }
                if (DE_to_EX.dep == 2) {
                    final_addr = DE_to_EX.instr_data.address + DE_to_EX.res;
                }
    			while ((final_addr & 0x3) != 0) {
    				final_addr = final_addr + 1;
    			}
    			EX_to_MEM.res = final_addr;
    	}
    	if ((DE_to_EX.decoded_instr.opcode == LDURB) || DE_to_EX.decoded_instr.opcode == LDURH)
    	{
            uint64_t final_addr;
            if ((DE_to_EX.dep == 0) || (DE_to_EX.dep == 1)) {
                 final_addr = (DE_to_EX.instr_data.address + CURRENT_STATE.REGS[DE_to_EX.instr_data.rn]);
            }
            if (DE_to_EX.dep == 2) {
                final_addr = DE_to_EX.instr_data.address + DE_to_EX.res;
            }
    		 EX_to_MEM.res = final_addr;
    	}
        if (DE_to_EX.decoded_instr.opcode == LSLI) {
            //printf("LSLI cond \n");
            if ((DE_to_EX.dep == 0) || (DE_to_EX.dep == 1)) {
                //printf("ex to mem res %li\n", EX_to_MEM.res);
                 EX_to_MEM.res = CURRENT_STATE.REGS[DE_to_EX.instr_data.rn] << (64 - DE_to_EX.instr_data.immediate);
            }
            if (DE_to_EX.dep == 2) {
                EX_to_MEM.res = DE_to_EX.res << (64 - DE_to_EX.instr_data.immediate);
                //printf("ex to mem res %li\n", EX_to_MEM.res);
                //printf("de to ex res %li\n", DE_to_EX.res);
            }
        }
        if (DE_to_EX.decoded_instr.opcode == LSRI) {
            if ((DE_to_EX.dep == 0) || (DE_to_EX.dep == 1)) {
                EX_to_MEM.res = CURRENT_STATE.REGS[DE_to_EX.instr_data.rn] >> DE_to_EX.instr_data.immediate;
            }
            if (DE_to_EX.dep == 2) {
                EX_to_MEM.res = DE_to_EX.res >> DE_to_EX.instr_data.immediate;
            }
        }
        if (DE_to_EX.decoded_instr.opcode == MOVZ) {
            EX_to_MEM.res = DE_to_EX.instr_data.immediate;
        }
        // could probably combine LDUR & STUR ?
        if ((DE_to_EX.decoded_instr.opcode == STUR64) ||
            (DE_to_EX.decoded_instr.opcode == STUR32)) {
                uint64_t final_addr;
                if ((DE_to_EX.dep == 0) || (DE_to_EX.dep == 1)) {
                     final_addr = (DE_to_EX.instr_data.address + CURRENT_STATE.REGS[DE_to_EX.instr_data.rn]);
                }
                if (DE_to_EX.dep == 2) {
                    final_addr = DE_to_EX.instr_data.address + DE_to_EX.res;
                }
    	     //printf("final address execute %lu\n", final_addr);
            	while((final_addr & 0x3) != 0) {
                    final_addr = final_addr + 1;
                }
    		//	printf("final address execute %lu\n", final_addr);
    			EX_to_MEM.res = final_addr;
    	}
        if ((DE_to_EX.decoded_instr.opcode == STURB || (DE_to_EX.decoded_instr.opcode == STURH))) {
            uint64_t final_addr;
            if ((DE_to_EX.dep == 0) || (DE_to_EX.dep == 1)) {
                 final_addr = (DE_to_EX.instr_data.address + CURRENT_STATE.REGS[DE_to_EX.instr_data.rn]);
            }
            if (DE_to_EX.dep == 2) {
                final_addr = DE_to_EX.instr_data.address + DE_to_EX.res;
            }
    	    EX_to_MEM.res = final_addr;
    	}
        if (DE_to_EX.decoded_instr.opcode == SUB) {
                if(DE_to_EX.dep == 0) {
                    EX_to_MEM.res = CURRENT_STATE.REGS[DE_to_EX.instr_data.rn] - CURRENT_STATE.REGS[DE_to_EX.instr_data.rm];
                }
                if(DE_to_EX.dep == 1) {
                    EX_to_MEM.res = CURRENT_STATE.REGS[DE_to_EX.instr_data.rn] - DE_to_EX.res;
                }
                if(DE_to_EX.dep == 2) {
                    EX_to_MEM.res = DE_to_EX.res - CURRENT_STATE.REGS[DE_to_EX.instr_data.rm];
                }
        }
        if (DE_to_EX.decoded_instr.opcode == SUBS || DE_to_EX.decoded_instr.opcode == CMP) {
            if(DE_to_EX.dep == 0) {
                    EX_to_MEM.res = CURRENT_STATE.REGS[DE_to_EX.instr_data.rn] - CURRENT_STATE.REGS[DE_to_EX.instr_data.rm];
                }
                if(DE_to_EX.dep == 1) {
                    EX_to_MEM.res = CURRENT_STATE.REGS[DE_to_EX.instr_data.rn] - DE_to_EX.res;
                }
                if(DE_to_EX.dep == 2) {
                    EX_to_MEM.res = DE_to_EX.res - CURRENT_STATE.REGS[DE_to_EX.instr_data.rm];
                }
                if (EX_to_MEM.res == 0) {
                    EX_to_MEM.flag_z = 1;
                    EX_to_MEM.flag_n = 0;
                }
                if (EX_to_MEM.res < 0) {
                    EX_to_MEM.flag_n = 1;
                    EX_to_MEM.flag_z = 0;
                }
                if (EX_to_MEM.res > 0) {
                    EX_to_MEM.flag_n = 0;
                    EX_to_MEM.flag_z = 0;
                }
        }
        if (DE_to_EX.decoded_instr.opcode == SUBI) {
            if ((DE_to_EX.dep == 0) || (DE_to_EX.dep == 1)) {
                EX_to_MEM.res = CURRENT_STATE.REGS[DE_to_EX.instr_data.rn] - DE_to_EX.instr_data.immediate;
            }
            if (DE_to_EX.dep == 2) {
                EX_to_MEM.res = DE_to_EX.res - DE_to_EX.instr_data.immediate;
            }
        }
        if (DE_to_EX.decoded_instr.opcode == SUBSI || DE_to_EX.decoded_instr.opcode == CMPI) {
            if ((DE_to_EX.dep == 0) || (DE_to_EX.dep == 1)) {
                EX_to_MEM.res = CURRENT_STATE.REGS[DE_to_EX.instr_data.rn] - DE_to_EX.instr_data.immediate;
            }
            if (DE_to_EX.dep == 2) {
                EX_to_MEM.res = DE_to_EX.res - DE_to_EX.instr_data.immediate;
            }
            if (EX_to_MEM.res == 0) {
                    EX_to_MEM.flag_z = 1;
                    EX_to_MEM.flag_n = 0;
                }
                if (EX_to_MEM.res < 0) {
                    EX_to_MEM.flag_n = 1;
                    EX_to_MEM.flag_z = 0;
                }
                if (EX_to_MEM.res > 0) {
                    EX_to_MEM.flag_n = 0;
                    EX_to_MEM.flag_z = 0;
                }
        }
        if (DE_to_EX.decoded_instr.opcode == MUL) {
            if(DE_to_EX.dep == 0) {
                EX_to_MEM.res = CURRENT_STATE.REGS[DE_to_EX.instr_data.rn] * CURRENT_STATE.REGS[DE_to_EX.instr_data.rm];
            }
            if(DE_to_EX.dep == 1) {
                EX_to_MEM.res = CURRENT_STATE.REGS[DE_to_EX.instr_data.rn] * DE_to_EX.res;
            }
            if(DE_to_EX.dep == 2) {
                EX_to_MEM.res = DE_to_EX.res * CURRENT_STATE.REGS[DE_to_EX.instr_data.rm];
            }
        }
        if (DE_to_EX.decoded_instr.opcode == HLT) {
    	    CURRENT_STATE.PC = hlt_pc;
    	}
    }
    // need to call bp_update() here 
    // need to check whether or not we're supposed to call during stalls 
    // but basically:
    // unconditional if br or b, else conditional 
    // 1 if taken, 2 if not taken for 
    int is_taken; 
    if(EX_to_MEM.branch_taken  == 1) {
        is_taken = 1;
    }
    if(EX_to_MEM.branch_taken == 2) {
        is_taken = 0;
    }
    int is_conditional; 
    if((EX_to_MEM.decoded_instr.opcode == B) || (EX_to_MEM.decoded_instr.opcode == BR)) {
        is_conditional = 0;
    }
    if((EX_to_MEM.decoded_instr.opcode == CBZ) ||
        (EX_to_MEM.decoded_instr.opcode == CBNZ) ||
        (EX_to_MEM.decoded_instr.opcode == BEQ) ||
        (EX_to_MEM.decoded_instr.opcode == BNE) ||
        (EX_to_MEM.decoded_instr.opcode == BGT) ||
        (EX_to_MEM.decoded_instr.opcode == BLT) ||
        (EX_to_MEM.decoded_instr.opcode == BGE) ||
        (EX_to_MEM.decoded_instr.opcode == BLE)) {
            is_conditional = 1;
    }
    // TODO: we need the pc from when the branch instr was in fetch, is this right?? 
    // TODO: double check that target pc should be branch pc 
    
    if((EX_to_MEM.decoded_instr.type == CB) ||
        (EX_to_MEM.decoded_instr.type == BI)) {
            printf("SOPHIE HERE IS WHERE WE'RE UPDATING\n");
            printf("branch pc: %lx\n", branch_pc);
            printf("CURRENT VAL OF PC BEFORE UPDATE %lx\n", CURRENT_STATE.PC);
            uint64_t tmp_tmp = CURRENT_STATE.PC;
            //CURRENT_STATE.PC = branch_pc;
            printf("CURRENT VAL OF PC AFTER UPDATE %lx\n", CURRENT_STATE.PC);
            printf("DE TO EX PC %lx\n", DE_to_EX.actually_pc);
            // why would it be de to ex actually pc for both? 
            if(((is_conditional == 1) && (is_taken == 1)) || (is_conditional == 0)) {
                //CURRENT_STATE.PC = branch_pc;
            }
            printf("CHECK BRANCH PC AGAIN %lx\n", branch_pc);
            bp_update(DE_to_EX.actually_pc, is_taken, is_conditional, EX_to_MEM.actually_pc, branch_pc);
             //check if we need to flush 
             // cases where we guessed wrong 
             // the predicted target destination does not match the actual target.
             if(branch_pc != DE_to_EX.predicted_pc) {
                 // flush decode 
                 printf("pc was mispredicted\n");
                 // let's just make it a bubble 
                 call_id = 1;
                 call_if = 1;
                 manually_set_pc = 1;
                 flush = 1;
                 return;
             }
             //i don't think we should have this condition but maybe I'm wrong?? 
             // check on this pls 
             if((is_taken == 0) && (branch_pc == DE_to_EX.predicted_pc)) {
                 manually_set_pc = 1;
                 flush = 1;
                 branch_pc = (DE_to_EX.actually_pc + 4);
             }
             else if(EX_to_MEM.btb_miss == 1) {
                 printf("BTB MISS: mispredicted as not a branch\n");
                  if((is_taken == 0) && (is_conditional == 1)) {
                      flush = 0;
                      manually_set_pc = 0;
                      return;
                  }
                  else {
                    flush = 1;
                    manually_set_pc = 1;
                    return;
                  }
             }
             else {
                 flush = 0;
                 manually_set_pc = 0;
             }
             printf("flush %i\n", flush);
        }
}

void pipe_stage_decode()
{
    //printf("WE MADE IT TO DECODE\n");
    // will happen 2nd cycle after mispredict 
    if(make_bub == 1) {
        DE_to_EX.decoded_instr.opcode = BUB;
        make_bub = 0;
        resetting_pc = 1;
        predicted_pc = branch_pc;
        return;
    }
    // will happen 1st cycle after midpredict 
    if(flush == 1) {
        DE_to_EX.decoded_instr.opcode = BUB;
        printf("We're flushing\n");
        //printf("%lu", CURRENT_STATE.PC);
        return;
    }
    if (IF_to_DE.actually_pc == 0) {
        //printf("you should be here\n");
        return;
    }
	//printf("decode pc %lx \n", IF_to_DE.actually_pc);
    DE_to_EX.predicted_pc = IF_to_DE.predicted_pc;
    DE_to_EX.pc = IF_to_DE.pc;
    //printf("IF TO DE pc getting messed up %lu\n", IF_to_DE.pc);
    DE_to_EX.flag_n = IF_to_DE.flag_n;
    DE_to_EX.flag_z = IF_to_DE.flag_z;
    DE_to_EX.actually_pc = IF_to_DE.actually_pc;
    DE_to_EX.btb_miss = IF_to_DE.btb_miss;
    DE_to_EX.branch_cond = 0;
    unsigned long opcode;

	// 6 bit opcodes
    opcode = pc >> 26;
	uint64_t b_addr = pc & 0x3FFFFFF;
	bit_26 b_addr_26;
	b_addr_26.bits = b_addr * 4;

	// B
	if (opcode == 0x5) {
        DE_to_EX.branch_cond = 1;
    	enum instr_type type =  BI;
   		enum op opcode = B;
		DE_to_EX.instr_data.address = b_addr_26.bits;
        //printf("DECODE instr_data.address %lu\n", DE_to_EX.instr_data.address);
		DE_to_EX.decoded_instr.opcode = opcode;
		DE_to_EX.decoded_instr.type = type;
		DE_to_EX.decoded_instr.data = DE_to_EX.instr_data;
	}

	// 8 bit opcodes
	opcode = pc >> 24;
	int64_t cb_address = (int64_t) pc;
	cb_address = (cb_address & 0xFFFFE0) >> 5;
	bit_19 cb_address_bits;
	cb_address_bits.bits = cb_address * 4;
	uint64_t cb_rt = pc & 0x1F;

  	// CBNZ
	if (opcode == 0xB5) {
        DE_to_EX.branch_cond = 1;
		enum instr_type type =  CB;
		enum op opcode = CBNZ;
		DE_to_EX.instr_data.address = cb_address_bits.bits;
		DE_to_EX.instr_data.rt = cb_rt;
		DE_to_EX.decoded_instr.opcode = opcode;
		DE_to_EX.decoded_instr.type = type;
		DE_to_EX.decoded_instr.data = DE_to_EX.instr_data;
	}
  	// CBZ
	if (opcode == 0xB4) {
        DE_to_EX.branch_cond = 1;
		enum instr_type type =  CB;
		enum op opcode = CBZ;
		DE_to_EX.instr_data.address = cb_address_bits.bits;
		DE_to_EX.instr_data.rt = cb_rt;
		DE_to_EX.decoded_instr.opcode = opcode;
		DE_to_EX.decoded_instr.type = type;
		DE_to_EX.decoded_instr.data = DE_to_EX.instr_data;
	}
	// B.cond
	if (opcode == 0x54) {
        DE_to_EX.branch_cond = 1;
		unsigned long condition_code = cb_rt;
		// BEQ
		if (condition_code == 0x0) {
			enum instr_type type =  CB;
			enum op opcode = BEQ;
            DE_to_EX.instr_data.address = cb_address_bits.bits;
    		DE_to_EX.instr_data.rt = cb_rt;
    		DE_to_EX.decoded_instr.opcode = opcode;
    		DE_to_EX.decoded_instr.type = type;
    		DE_to_EX.decoded_instr.data = DE_to_EX.instr_data;
		}
		// BNE
		if (condition_code == 0x1) {
			enum instr_type type =  CB;
			enum op opcode = BNE;
            DE_to_EX.instr_data.address = cb_address_bits.bits;
    		DE_to_EX.instr_data.rt = cb_rt;
    		DE_to_EX.decoded_instr.opcode = opcode;
    		DE_to_EX.decoded_instr.type = type;
    		DE_to_EX.decoded_instr.data = DE_to_EX.instr_data;
		}
		// BGT
		if (condition_code == 0xC) {
			enum instr_type type =  CB;
			enum op opcode = BGT;
            DE_to_EX.instr_data.address = cb_address_bits.bits;
    		DE_to_EX.instr_data.rt = cb_rt;
    		DE_to_EX.decoded_instr.opcode = opcode;
    		DE_to_EX.decoded_instr.type = type;
    		DE_to_EX.decoded_instr.data = DE_to_EX.instr_data;
		}
		// BLT
		if (condition_code == 0xB) {
			enum instr_type type =  CB;
			enum op opcode = BLT;
            DE_to_EX.instr_data.address = cb_address_bits.bits;
    		DE_to_EX.instr_data.rt = cb_rt;
    		DE_to_EX.decoded_instr.opcode = opcode;
    		DE_to_EX.decoded_instr.type = type;
    		DE_to_EX.decoded_instr.data = DE_to_EX.instr_data;
		}
		// BGE
		if (condition_code == 0xA) {
			enum instr_type type =  CB;
			enum op opcode = BGE;
            DE_to_EX.instr_data.address = cb_address_bits.bits;
    		DE_to_EX.instr_data.rt = cb_rt;
    		DE_to_EX.decoded_instr.opcode = opcode;
    		DE_to_EX.decoded_instr.type = type;
    		DE_to_EX.decoded_instr.data = DE_to_EX.instr_data;
		}
		// BLE
		if (condition_code == 0xD) {
			enum instr_type type =  CB;
			enum op opcode = BLE;
            DE_to_EX.instr_data.address = cb_address_bits.bits;
    		DE_to_EX.instr_data.rt = cb_rt;
    		DE_to_EX.decoded_instr.opcode = opcode;
    		DE_to_EX.decoded_instr.type = type;
    		DE_to_EX.decoded_instr.data = DE_to_EX.instr_data;
		}
	}
	// 9 bit opcodes
	opcode = pc >> 23;
	uint64_t iw_immediate = ((pc >> 5) & 0xFFFF);
	uint64_t iw_op2 = (pc >> 21) & 0x3;
	uint64_t iw_rd = pc & 0x1F;

	// MOVZ
	if (opcode == 0x1A5) {
		enum instr_type type =  IW;
		enum op opcode = MOVZ;
		DE_to_EX.instr_data.immediate = iw_immediate;
		DE_to_EX.instr_data.op2 = iw_op2;
		DE_to_EX.instr_data.rd = iw_rd;
		DE_to_EX.decoded_instr.opcode = opcode;
		DE_to_EX.decoded_instr.type = type;
		DE_to_EX.decoded_instr.data = DE_to_EX.instr_data;
	}

	// 10 bit opcodes
	opcode = pc >> 22;
	uint64_t i_immediate = (pc >> 10) & 0xFFF;
	uint64_t i_rn = (pc >> 5) & 0x1F;
	uint64_t i_rd = pc & 0x1F;

	// ADDI
	if (opcode == 0x244) {
		enum instr_type type =  I;
		enum op opcode = ADDI;
		DE_to_EX.instr_data.immediate = i_immediate;
		DE_to_EX.instr_data.rn = i_rn;
		DE_to_EX.instr_data.rd = i_rd;
		DE_to_EX.decoded_instr.opcode = opcode;
		DE_to_EX.decoded_instr.type = type;
		DE_to_EX.decoded_instr.data = DE_to_EX.instr_data;
	}
	// ADDIS
	if (opcode == 0x2C4) {
		enum instr_type type =  I;
		enum op opcode = ADDSI;
        DE_to_EX.instr_data.immediate = i_immediate;
		DE_to_EX.instr_data.rn = i_rn;
		DE_to_EX.instr_data.rd = i_rd;
		DE_to_EX.decoded_instr.opcode = opcode;
		DE_to_EX.decoded_instr.type = type;
		DE_to_EX.decoded_instr.data = DE_to_EX.instr_data;
	}
	// SUBI
	if (opcode == 0x344) {
		enum instr_type type =  I;
		enum op opcode = SUBI;
        DE_to_EX.instr_data.immediate = i_immediate;
        DE_to_EX.instr_data.rn = i_rn;
        DE_to_EX.instr_data.rd = i_rd;
        DE_to_EX.decoded_instr.opcode = opcode;
        DE_to_EX.decoded_instr.type = type;
        DE_to_EX.decoded_instr.data = DE_to_EX.instr_data;
	}
	// SUBSI / CMPI
	if (opcode == 0x3C4) {
		enum instr_type type =  I;
		enum op opcode = SUBSI;
        DE_to_EX.instr_data.immediate = i_immediate;
        DE_to_EX.instr_data.rn = i_rn;
        DE_to_EX.instr_data.rd = i_rd;
        DE_to_EX.decoded_instr.opcode = opcode;
        DE_to_EX.decoded_instr.type = type;
        DE_to_EX.decoded_instr.data = DE_to_EX.instr_data;
	}
	if (opcode == 0x34D) {
		// LSRI
		if ((i_immediate & 0x3F) == 0x3F) {
			enum instr_type type =  I;
			enum op opcode = LSRI;
			DE_to_EX.instr_data.immediate = (i_immediate >> 6) & 0x3F;
            DE_to_EX.instr_data.rn = i_rn;
            DE_to_EX.instr_data.rd = i_rd;
            DE_to_EX.decoded_instr.opcode = opcode;
            DE_to_EX.decoded_instr.type = type;
            DE_to_EX.decoded_instr.data = DE_to_EX.instr_data;
		}
		// LSLI
		else {
			enum instr_type type =  I;
			enum op opcode = LSLI;
            DE_to_EX.instr_data.immediate = (i_immediate >> 6) & 0x3F;
            DE_to_EX.instr_data.rn = i_rn;
            DE_to_EX.instr_data.rd = i_rd;
            DE_to_EX.decoded_instr.opcode = opcode;
            DE_to_EX.decoded_instr.type = type;
            DE_to_EX.decoded_instr.data = DE_to_EX.instr_data;
		}
	}

	// 11 bit opcodes
	opcode = pc >> 21;
	uint64_t r_rm = (pc >> 16) & 0x1F;
	uint64_t r_shamt = (pc >> 10) & 0x3F;
	uint64_t r_rn = (pc >> 5) & 0x1F;
	uint64_t r_rd = pc & 0x1F;
	uint64_t d_addoffset = (pc >> 12) & 0x1FF;
	uint64_t d_op2 = (pc >> 10) & 0x3;
	uint64_t d_rn = (pc >> 5) & 0x1F;
	uint64_t d_rt = pc & 0x1F;

	// ADD
	if (opcode == 0x458) {
		enum instr_type type =  R;
    	enum op opcode = ADD;
        DE_to_EX.decoded_instr.opcode = opcode;
        DE_to_EX.decoded_instr.type = type;
		DE_to_EX.instr_data.rm = r_rm;
		DE_to_EX.instr_data.shamt = r_shamt;
		DE_to_EX.instr_data.rn = r_rn;
		DE_to_EX.instr_data.rd = r_rd;
		DE_to_EX.decoded_instr.data = DE_to_EX.instr_data;
	}
	// ADDS
	if (opcode == 0x558) {
		enum instr_type type =  R;
    	enum op opcode = ADDS;
        DE_to_EX.decoded_instr.opcode = opcode;
        DE_to_EX.decoded_instr.type = type;
		DE_to_EX.instr_data.rm = r_rm;
		DE_to_EX.instr_data.shamt = r_shamt;
		DE_to_EX.instr_data.rn = r_rn;
		DE_to_EX.instr_data.rd = r_rd;
		DE_to_EX.decoded_instr.data = DE_to_EX.instr_data;
	}
	// AND
	if (opcode == 0x450) {
		enum instr_type type =  R;
    	enum op opcode = AND;
        DE_to_EX.decoded_instr.opcode = opcode;
        DE_to_EX.decoded_instr.type = type;
		DE_to_EX.instr_data.rm = r_rm;
		DE_to_EX.instr_data.shamt = r_shamt;
		DE_to_EX.instr_data.rn = r_rn;
		DE_to_EX.instr_data.rd = r_rd;
		DE_to_EX.decoded_instr.data = DE_to_EX.instr_data;
	}
	// ANDS
	if (opcode == 0x750) {
		enum instr_type type =  R;
    	enum op opcode = ANDS;
        DE_to_EX.decoded_instr.opcode = opcode;
        DE_to_EX.decoded_instr.type = type;
		DE_to_EX.instr_data.rm = r_rm;
		DE_to_EX.instr_data.shamt = r_shamt;
		DE_to_EX.instr_data.rn = r_rn;
		DE_to_EX.instr_data.rd = r_rd;
		DE_to_EX.decoded_instr.data = DE_to_EX.instr_data;
	}
	// EOR
	if (opcode == 0x650) {
		enum instr_type type =  R;
    	enum op opcode = EOR;
        DE_to_EX.decoded_instr.opcode = opcode;
        DE_to_EX.decoded_instr.type = type;
		DE_to_EX.instr_data.rm = r_rm;
		DE_to_EX.instr_data.shamt = r_shamt;
		DE_to_EX.instr_data.rn = r_rn;
		DE_to_EX.instr_data.rd = r_rd;
		DE_to_EX.decoded_instr.data = DE_to_EX.instr_data;
	}
	// ORR
	if (opcode == 0x550) {
		enum instr_type type =  R;
	    	enum op opcode = ORR;
            DE_to_EX.decoded_instr.opcode = opcode;
            DE_to_EX.decoded_instr.type = type;
    		DE_to_EX.instr_data.rm = r_rm;
    		DE_to_EX.instr_data.shamt = r_shamt;
    		DE_to_EX.instr_data.rn = r_rn;
    		DE_to_EX.instr_data.rd = r_rd;
    		DE_to_EX.decoded_instr.data = DE_to_EX.instr_data;
	}
	// LDUR64
	if (opcode == 0x7C2) {
		enum instr_type type =  D;
		enum op opcode = LDUR64;
		DE_to_EX.instr_data.address = d_addoffset;
		DE_to_EX.instr_data.op2 = d_op2;
		DE_to_EX.instr_data.rn = d_rn;
		DE_to_EX.instr_data.rt = d_rt;
		DE_to_EX.decoded_instr.opcode = opcode;
		DE_to_EX.decoded_instr.type = type;
		DE_to_EX.decoded_instr.data = DE_to_EX.instr_data;
	}
	// LDUR32
	if (opcode == 0x5C2) {
		enum instr_type type =  D;
		enum op opcode = LDUR32;
        DE_to_EX.instr_data.address = d_addoffset;
		DE_to_EX.instr_data.op2 = d_op2;
		DE_to_EX.instr_data.rn = d_rn;
		DE_to_EX.instr_data.rt = d_rt;
		DE_to_EX.decoded_instr.opcode = opcode;
		DE_to_EX.decoded_instr.type = type;
		DE_to_EX.decoded_instr.data = DE_to_EX.instr_data;
    }
	// LDURB
	if (opcode == 0x1C2) {
		enum instr_type type =  D;
		enum op opcode = LDURB;
        DE_to_EX.instr_data.address = d_addoffset;
		DE_to_EX.instr_data.op2 = d_op2;
		DE_to_EX.instr_data.rn = d_rn;
		DE_to_EX.instr_data.rt = d_rt;
		DE_to_EX.decoded_instr.opcode = opcode;
		DE_to_EX.decoded_instr.type = type;
		DE_to_EX.decoded_instr.data = DE_to_EX.instr_data;
	}
	// LDURH
	if (opcode == 0x3C2) {
		enum instr_type type =  D;
		enum op opcode = LDURH;
        DE_to_EX.instr_data.address = d_addoffset;
		DE_to_EX.instr_data.op2 = d_op2;
		DE_to_EX.instr_data.rn = d_rn;
		DE_to_EX.instr_data.rt = d_rt;
		DE_to_EX.decoded_instr.opcode = opcode;
		DE_to_EX.decoded_instr.type = type;
		DE_to_EX.decoded_instr.data = DE_to_EX.instr_data;
	}
	// STUR64
	if (opcode == 0x7C0) {
		enum instr_type type =  D;
		enum op opcode = STUR64;
        DE_to_EX.instr_data.address = d_addoffset;
		DE_to_EX.instr_data.op2 = d_op2;
		DE_to_EX.instr_data.rn = d_rn;
		DE_to_EX.instr_data.rt = d_rt;
		DE_to_EX.decoded_instr.opcode = opcode;
		DE_to_EX.decoded_instr.type = type;
		DE_to_EX.decoded_instr.data = DE_to_EX.instr_data;
	}
	// STUR32
	 if (opcode == 0x5C0) {
		enum instr_type type =  D;
 		enum op opcode = STUR32;
        DE_to_EX.instr_data.address = d_addoffset;
		DE_to_EX.instr_data.op2 = d_op2;
		DE_to_EX.instr_data.rn = d_rn;
		DE_to_EX.instr_data.rt = d_rt;
		DE_to_EX.decoded_instr.opcode = opcode;
		DE_to_EX.decoded_instr.type = type;
		DE_to_EX.decoded_instr.data = DE_to_EX.instr_data;
    }
	// STURB
	if (opcode == 0x1C0) {
		enum instr_type type =  D;
		enum op opcode = STURB;
        DE_to_EX.instr_data.address = d_addoffset;
		DE_to_EX.instr_data.op2 = d_op2;
		DE_to_EX.instr_data.rn = d_rn;
		DE_to_EX.instr_data.rt = d_rt;
		DE_to_EX.decoded_instr.opcode = opcode;
		DE_to_EX.decoded_instr.type = type;
		DE_to_EX.decoded_instr.data = DE_to_EX.instr_data;
	}
	// STURH
	if (opcode == 0x3C0) {
		enum instr_type type =  D;
		enum op opcode = STURH;
        DE_to_EX.instr_data.address = d_addoffset;
		DE_to_EX.instr_data.op2 = d_op2;
		DE_to_EX.instr_data.rn = d_rn;
		DE_to_EX.instr_data.rt = d_rt;
		DE_to_EX.decoded_instr.opcode = opcode;
		DE_to_EX.decoded_instr.type = type;
		DE_to_EX.decoded_instr.data = DE_to_EX.instr_data;
	}
	// SUB
	if (opcode == 0x658) {
		enum instr_type type =  R;
    	enum op opcode = SUB;
        DE_to_EX.decoded_instr.opcode = opcode;
        DE_to_EX.decoded_instr.type = type;
		DE_to_EX.instr_data.rm = r_rm;
		DE_to_EX.instr_data.shamt = r_shamt;
		DE_to_EX.instr_data.rn = r_rn;
		DE_to_EX.instr_data.rd = r_rd;
		DE_to_EX.decoded_instr.data = DE_to_EX.instr_data;
	}
	// SUBS // CMP
	if (opcode == 0x758) {
		enum instr_type type =  R;
    	enum op opcode = SUBS;
        DE_to_EX.decoded_instr.opcode = opcode;
        DE_to_EX.decoded_instr.type = type;
		DE_to_EX.instr_data.rm = r_rm;
		DE_to_EX.instr_data.shamt = r_shamt;
		DE_to_EX.instr_data.rn = r_rn;
		DE_to_EX.instr_data.rd = r_rd;
		DE_to_EX.decoded_instr.data = DE_to_EX.instr_data;
	}
	// MUL
	if (opcode == 0x4D8) {
		enum instr_type type =  R;
    	enum op opcode = MUL;
        DE_to_EX.decoded_instr.opcode = opcode;
        DE_to_EX.decoded_instr.type = type;
		DE_to_EX.instr_data.rm = r_rm;
		DE_to_EX.instr_data.shamt = r_shamt;
		DE_to_EX.instr_data.rn = r_rn;
		DE_to_EX.instr_data.rd = r_rd;
		DE_to_EX.decoded_instr.data = DE_to_EX.instr_data;
	}
	// HLT
	if (opcode == 0x6A2) {
		enum instr_type type =  IW;
		enum op opcode = HLT;
        	DE_to_EX.decoded_instr.opcode = opcode;
        	DE_to_EX.decoded_instr.type = type;
		DE_to_EX.pc = IF_to_DE.pc + 8;
		uint64_t immediate = ((pc >> 5) & 0xFFFF);
		DE_to_EX.instr_data.immediate = immediate;
		DE_to_EX.decoded_instr.data = DE_to_EX.instr_data;
		hlt_pc = CURRENT_STATE.PC;
		CURRENT_STATE.PC = hlt_pc;
	}
	// BR
	if (opcode == 0x6B0) {
		enum instr_type type =  R;
        DE_to_EX.branch_cond = 1;
	    enum op opcode = BR;
        DE_to_EX.decoded_instr.opcode = opcode;
        DE_to_EX.decoded_instr.type = type;
		DE_to_EX.instr_data.rm = r_rm;
		DE_to_EX.instr_data.shamt = r_shamt;
		DE_to_EX.instr_data.rn = r_rn;
		DE_to_EX.instr_data.rd = r_rd;
		DE_to_EX.decoded_instr.data = DE_to_EX.instr_data;
	}
	//printf("%u\n", DE_to_EX.decoded_instr.opcode);
	//printf("decode de to ex rn  %lu\n", DE_to_EX.instr_data.rn);
      if((DE_to_EX.decoded_instr.type == CB ||
        DE_to_EX.decoded_instr.type == BI)) {
            printf("A BRANCH INSTR IS IN DECODE\n");
        }
    
}

void pipe_stage_fetch()
{
    if(flush == 1) {
        make_bub = 1;
        flush = 0;
        return;
    }
    
    uint64_t temp_pc = CURRENT_STATE.PC;
    if(resetting_pc == 1) {
        was_reset = 1;
        CURRENT_STATE.PC = predicted_pc;
        tmp_pc_reset = CURRENT_STATE.PC;
        temp_pc += 4;
        resetting_pc = 0;
        printf("WE ARE RESETTING the PC\n");
        printf("RESET PC VALUE %lx\n", CURRENT_STATE.PC);
    }
    //printf("PC 1 %lu\n", CURRENT_STATE.PC);
    //printf("\nHERE %lu\n", CURRENT_STATE.PC);
    pc = mem_read_32(CURRENT_STATE.PC);
    IF_to_DE.flag_z = CURRENT_STATE.FLAG_Z;
    IF_to_DE.flag_n = CURRENT_STATE.FLAG_N;
    IF_to_DE.pc = pc;
    IF_to_DE.actually_pc = CURRENT_STATE.PC;
    //printf("HERE %lu\n", CURRENT_STATE.PC);
    bp_predict(CURRENT_STATE.PC);
    IF_to_DE.predicted_pc = predicted_pc;
    IF_to_DE.btb_miss = btb_miss_temp; 
    printf("predicted PC: %lx\n", predicted_pc);
    printf("\n CURRENT STATE PC at end of fetch %lx\n", CURRENT_STATE.PC);
    //printf("HERE %lu\n", CURRENT_STATE.PC);
    /*
    if(stat_cycles == 89) {
        predicted_pc += 4;
    }
    */
	// predict next PC 
	//bp_predict(CURRENT_STATE.PC);
    //printf("pc %u\n", mem_read_32(CURRENT_STATE.PC));
    //printf("fetch %lx\n", CURRENT_STATE.PC);
    //printf("Final fucking pc");
    //printf("%lu", CURRENT_STATE.PC);

}
