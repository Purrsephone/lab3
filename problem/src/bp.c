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
    // btb contains 1024 entries is indexed by bits [11:2] of the PC
    // gets bits 11:2 of PC which is the index used for btb
    uint32_t btb_index = ((pc_fetch >> 2) & 0b1111111111);
    // gets bits 9:2 of PC then XORs with ght and this index is used for pht 
    uint32_t pht_index = (((pc_fetch >> 2) & 0b11111111) ^ bp.gshare.ghr);

    // MISS cases 

    // if address tag is not equal to pc, just pc + 4
    if (bp.btb.entries[btb_index].tag != pc_fetch) {
        predicted_pc = pc_fetch + 4;
        btb_miss = 1;
        return;
    }
    // if valid bit is not set, just pc + 4 
    else if (bp.btb.entries[btb_index].valid != 1) {
        predicted_pc = pc_fetch + 4;
        btb_miss = 1;
        return;
    }
    // HIT case
    else {
        btb_miss = 0;
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
    // if branch was unconditional, skip the pht and ghr updating steps 
    if(conditional != 0) {

        // gets bits 9:2 of PC then XORs with ght and this index is used for pht 
        uint32_t pht_index = (((current_pc >> 2) & 0b11111111) ^ bp.gshare.ghr);

        // save current_pht_val 
        int current_pht_val = bp.gshare.pht[pht_index];

        // branch not taken cond 
        if(branch_taken == 0) {
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
        short val_ghr = bp.gshare.ghr;
        bp.gshare.ghr = (val_ghr << 1);
        // left shifting always replaces last digit with a 0 
        // if branch was not taken, we can leave ghr alone bc LSB is already 0
        if(branch_taken == 0){
            ;
        }
        // if branch was taken, we must change LSB of ghr to 1 by | with 0b1 
        if(branch_taken == 1) {
            short val_ghr_final = bp.gshare.ghr;
            bp.gshare.ghr = (val_ghr_final | 0b1);
        }
    }

    // update btb
    // btb contains 1024 entries is indexed by bits [11:2] of the PC
    // gets bits 11:2 of PC which is the index used for btb
    // which PC should we use to index 
    uint32_t btb_index = ((branch_pc_from_fetch >> 2) & 0b1111111111);

    //ensure that last 2 bits of branch target pc are 00 (per handout)
    uint64_t branch_target_pc_val = branch_target_pc;
    branch_target_pc = (branch_target_pc_val << 2);

    // make an entry struct with info 
    entry_t new_entry = {
        .tag = branch_pc_from_fetch,
        .valid = 1,
        .cond = conditional,
        .target = branch_target_pc
    };

    // now add this entry to the proper index 
    bp.btb.entries[btb_index] = new_entry;

}
