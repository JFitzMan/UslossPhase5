
#include "usloss.h"
#include "vm.h"
#include <phase1.h>
#include "phase2.h" 
#include <phase3.h>
#include <phase4.h>
#include <phase5.h>
#include <stdio.h>
#include <string.h>

#define DEBUG 0

extern int debugflag;
//extern Process processes[50];
extern int mmuInitialized;

void
p1_fork(int pid)
{
    //this prevents the earlier phases from calling this before init is called
    if(mmuInitialized){
        if (DEBUG)
            USLOSS_Console("p1_fork() called: pid = %d, MMU online\n", pid);

        //initialize process table entry
        processes[pid].pid = pid;
        processes[pid%MAXPROC].numPages = vmStats.pages;
        processes[pid%MAXPROC].pageTable = malloc(sizeof(PTE)*(vmStats.pages));

        if (processes[pid%MAXPROC].pageTable == NULL){
            USLOSS_Console("p1_fork() malloc failed\n");
        }

        
        //initialize page table
        int i;
        for (i = 0; i < vmStats.pages; i++){
            processes[pid%MAXPROC].pageTable[i].state = UNUSED;
            processes[pid%MAXPROC].pageTable[i].frame = -1;
            processes[pid%MAXPROC].pageTable[i].diskBlock = -1;
            processes[pid%MAXPROC].pageTable[i].pageNum = i;
            processes[pid%MAXPROC].pageTable[i].beenRef = 0;
        }
    }
    else{
        if (DEBUG)
            USLOSS_Console("p1_fork() called: pid = %d\n", pid);
    }

} /* p1_fork */

void
p1_switch(int old, int new)
{
    if (DEBUG)
        USLOSS_Console("p1_switch() called: old = %d, new = %d\n", old, new);

    if(mmuInitialized){
        vmStats.switches++;
        if (processes[old].pid == old && processes[new].pid == new){
        char toWrite [USLOSS_MmuPageSize() + 1];
        int error1 = 0;
        
        //unmap any page currently in a frame. Just need page #
        int i;
        for (i = 0; i < vmStats.pages; i++){
            if (processes[old].pageTable[i].state == INFRAME){
                int toUnmap = processes[old].pageTable[i].pageNum;
                if (DEBUG){
                    USLOSS_Console("p1_switch(): about to unmap page %d from frame %d\n", toUnmap, processes[old].pageTable[i].frame);
                }

                memcpy(toWrite, vmRegion+(USLOSS_MmuPageSize()*i), USLOSS_MmuPageSize());

                error1 = USLOSS_MmuUnmap(TAG, toUnmap);
                if (error1 != USLOSS_MMU_OK){
                  USLOSS_Console("p1_switch(): couldn't unmap MMU, status %d\n", error1);
                  abort();
                }

                vmStats.freeFrames--;
            }
        }//end for

        //check new process page table, see if any of its pages are mapped to frames.
        long accessPtr = 0;
        for (i = 0; i < vmStats.pages; i++){
            if (processes[new].pageTable[i].state == INFRAME){

                if (DEBUG){
                    USLOSS_Console("p1_switch(): about to map page %d to frame %d\n", i, processes[new].pageTable[i].frame);
                }
                error1 = USLOSS_MmuMap(TAG, i, processes[new].pageTable[i].frame, USLOSS_MMU_PROT_RW);
                if (error1 != USLOSS_MMU_OK){
                    USLOSS_Console("p1_switch(): couldn't map MMU, status %d\n", error1);
                    abort();
                }

                vmStats.freeFrames++;
                /*
                error = USLOSS_MmuGetAccess(frameToMap, &accessPtr);
                accessPtr = accessPtr&USLOSS_MMU_REF;
                frameTable[frameToMap].dirty = accessPtr&USLOSS_MMU_DIRTY;
                USLOSS_MmuSetAccess(frameToMap, accessPtr);
*/
                if ( frameTable[processes[new].pageTable[i].frame].ref != -1 && frameTable[processes[new].pageTable[i].frame].dirty != -1){
                    accessPtr = frameTable[processes[new].pageTable[i].frame].ref + frameTable[processes[new].pageTable[i].frame].dirty;
                    
                    USLOSS_MmuSetAccess(processes[new].pageTable[i].frame, accessPtr);
                    if (DEBUG){
                        USLOSS_Console("p1_switch(): ref = %d and dirty = %d\n", accessPtr&USLOSS_MMU_REF, accessPtr&USLOSS_MMU_DIRTY);
                    }

                }
    
                
            }
        }
    }
    }
    else{
        //do nothing, mmu isn't initialized yet
    }
} /* p1_switch */

void
p1_quit(int pid)
{
    if (DEBUG)
        USLOSS_Console("p1_quit() called: pid = %d\n", pid);

    //if the mmu is initialized and the quitting process has a page table
    if( mmuInitialized && processes[pid].pid == pid ){

        //check if the process has any disk blocks to free
        int i;
        for (i = 0; i < processes[pid].numPages; i++){
            if(processes[pid%MAXPROC].pageTable[i].diskBlock != -1){
                //free disk block
                freeBlocks[processes[pid%MAXPROC].pageTable[i].diskBlock] = 0;
                vmStats.freeDiskBlocks++;
            }
        }

        //free the page table
        free(processes[pid%MAXPROC].pageTable);

        //clear the process table
        processes[pid%MAXPROC].pid = -1;



    }
} /* p1_quit */

