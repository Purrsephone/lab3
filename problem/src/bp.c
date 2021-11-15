/*
 * ARM pipeline timing simulator
 *
 * CMSC 22200, Fall 2016
 * Gushu Li and Reza Jokar
 */

#include "bp.h"
#include "pipe.h"
#include <stdlib.h>
#include <stdio.h>

// initialize bp 
bp_t bp;



// should take fetch pc as an argument 
// should be called during fetch stage 
// need to do something with pc_prediction ( can't be a local variable )
void bp_predict(uint64_t pc_fetch)
{
    printf("INSIDE BP PREDICT\n");
    // btb contains 1024 entries is indexed by bits [11:2] of the PC
    // gets bits 11:2 of PC which is the index used for btb
    uint32_t btb_index = ((pc_fetch >> 2) & 0b1111111111);
    // gets bits 9:2 of PC then XORs with ght and this index is used for pht 
    uint32_t pht_index = (((pc_fetch >> 2) & 0b11111111) ^ bp.gshare.ghr);

    // MISS cases 

    // if address tag is not equal to pc, just pc + 4
    if (bp.btb.entries[btb_index].tag != pc_fetch) {
        predicted_pc = pc_fetch + 4;
        btb_miss_temp = 1;
        // falling into this condition and we don't want that
        printf("BTB_INDEX: %u\n", btb_index); 
        printf("PC FETCH: %lx\n", pc_fetch);
        printf("ADDR TAG: %lx\n", bp.btb.entries[btb_index].tag);
        printf("MISS: ADDRESS TAG NOT EQUAL\n");

        return;
    }
    // if valid bit is not set, just pc + 4 
    else if (bp.btb.entries[btb_index].valid != 1) {
        predicted_pc = pc_fetch + 4;
        btb_miss_temp = 1;
        printf("MISS: VALID BIT NOT SET\n");
        return;
    }
    // HIT case
    else {
        printf("HIT\n");
        btb_miss_temp = 0;
        printf("BTB_INDEX: %u\n", btb_index); 
        printf("PC FETCH: %lx\n", pc_fetch);
        printf("ADDR TAG: %lx\n", bp.btb.entries[btb_index].tag);
        printf("BTB MISS TEMP AT END OF PREDICT %i\n", btb_miss_temp);
        // if branch is unconditional, then take target branch
        if (bp.btb.entries[btb_index].cond == 0) {
            predicted_pc = bp.btb.entries[btb_index].target;
            return;
        }
        // if gshare predictor is 10 or 11, then take target branch 
        else if ((bp.gshare.pht[pht_index] == 2) || (bp.gshare.pht[pht_index] == 3)) {
            predicted_pc = bp.btb.entries[btb_index].target;
            return;
        }
        // otherwise, just return pc + 4
        else {
            printf("HIT BUT JUST STATUS QUO\n");
            predicted_pc = pc_fetch + 4;
            return;
        }
    }
}

// should be called during the execute stage
// current_pc is current value of pc 
// branch_taken is a boolean, 1 if taken, 0 if not 
// we need to figure out if we took the branch or nah 
// update pht entry accordingly 
// conditional is whether the branch was conditional (1) or unconditional (0)
// branch_pc_from_fetch is the pc when the branch instruction was in the fetch stage **** 
void bp_update(uint64_t current_pc, int branch_taken, int conditional, uint64_t branch_pc_from_fetch, uint64_t branch_target_pc)
{   
    printf("LET'S CHECK ON CURRENT_PC %lu\n", current_pc);
    printf("WE ARE INSIDE BP UDPATE BITCHES\n");
    // if branch was unconditional, skip the pht and ghr updating steps 
    if(conditional != 0) {
        printf("WE ARE CONDITIONAL\n");
        // gets bits 9:2 of PC then XORs with ght and this index is used for pht 
        uint32_t pht_index = (((branch_pc_from_fetch >> 2) & 0b11111111) ^ bp.gshare.ghr);
        uint64_t test = 4194304;
        for(int j = 0; j < 20; j++) {
            uint32_t res = (((test >>2 ) & 0b11111111) ^ bp.gshare.ghr);
            if(res == 7) {
                printf("YAY\n");
                printf("%lu", test);
                break;
            }
            test += 4;
        }
        printf("GHR %i\n", bp.gshare.ghr);
        printf("current pc %lu\n", current_pc);
        printf("current pc shifted %lu\n", (current_pc >> 2));
        printf("PHT INDX %u\n", pht_index);

        // save current_pht_val 
        int current_pht_val = bp.gshare.pht[pht_index];

        // branch not taken cond 
        if(branch_taken == 0) {
            printf("WE ARE NOT TAKING THE BRANCH\n");
            // if at strongly not taken and was not taken, no change 
            if(current_pht_val == 0) {
                ;
            }
            // if at weakly not taken and was not taken, change to strongly not taken 
            if(current_pht_val == 1) {
                bp.gshare.pht[pht_index] = 0;
            }
            // if at weakly taken and was not taken, change to weakly not taken 
            if(current_pht_val == 2) {
                bp.gshare.pht[pht_index] = 1;
            }
            // if at strongly taken and was not taken, change to weakly taken 
            if(current_pht_val == 3) {
                bp.gshare.pht[pht_index] = 2;
            }
        }
        // branch taken cond 
        if(branch_taken == 1) {
            printf("WE ARE TAKING THE BRANCH\n");
            // if at strongly not taken and was taken, change to weakly not taken 
            if(current_pht_val == 0) {
                bp.gshare.pht[pht_index] = 1;
            }
            // if at weakly not taken and was taken, change to weakly taken 
            if(current_pht_val == 1) {
                bp.gshare.pht[pht_index] = 2;
            }
            // if at weakly taken and was taken, change to strongly taken 
            if(current_pht_val == 2) {
                bp.gshare.pht[pht_index] = 3;
            }
            // if at strongly taken and was taken, no change 
            if(current_pht_val == 3) {
                ;
            }

        }

        // update the GHR, the least significant bit will be 0 or 1 based on branch_taken arg 
        // first, left shift the ghr to delete oldest entry and make room for newest entry 
        //int val_ghr = bp.gshare.ghr;
        bp.gshare.ghr = (bp.gshare.ghr << 1);
        bp.gshare.ghr = (bp.gshare.ghr & 0b011111111);
        // left shifting always replaces last digit with a 0 
        // if branch was not taken, we can leave ghr alone bc LSB is already 0
        if(branch_taken == 0){
            ;
        }
        // if branch was taken, we must change LSB of ghr to 1 by | with 0b1 
        if(branch_taken == 1) {
            printf("val_ghr_final %i\n", bp.gshare.ghr );

            bp.gshare.ghr = (bp.gshare.ghr  | 0b00000001);
        }
    }

    // update btb
    // btb contains 1024 entries is indexed by bits [11:2] of the PC
    // gets bits 11:2 of PC which is the index used for btb
    // which PC should we use to index 
    printf("pc current: %lu\n", current_pc);
    printf("pc we're using to index: %lu\n", branch_pc_from_fetch);
    uint32_t btb_index = ((branch_pc_from_fetch >> 2) & 0b1111111111);

    //ensure that last 2 bits of branch target pc are 00 (per handout)
    uint64_t branch_target_pc_val = branch_target_pc;
    //branch_target_pc = (branch_target_pc_val << 2);

    // make an entry struct with info 
    printf("BRANCH TARGET PC %lu\n", branch_target_pc);
    entry_t new_entry = {
        .tag = branch_pc_from_fetch,
        .valid = 1,
        .cond = conditional,
        .target = branch_target_pc
    };

    // now add this entry to the proper index 
    bp.btb.entries[btb_index] = new_entry;
    printf("BTB INDEX: %u\n", btb_index);
    //printf("lu\n")

}
