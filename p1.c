
#include "usloss.h"
#include "vm.h"
#include <phase1.h>
#include "phase2.h" 
#include <phase3.h>
#include <phase4.h>
#include <phase5.h>
#include <stdio.h>
#include <string.h>

#define DEBUG 1

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
                    USLOSS_Console("p1_switch(): about to unmap page %d\n", toUnmap);
                }

                memcpy(toWrite, vmRegion+(USLOSS_MmuPageSize()*i), USLOSS_MmuPageSize());

                error1 = USLOSS_MmuUnmap(TAG, toUnmap);
                if (error1 != USLOSS_MMU_OK){
                  USLOSS_Console("p1_switch(): couldn't unmap MMU, status %d\n", error1);
                  abort();
                }
                /*
                frameTable[i].state = UNUSED;
                frameTable[i].pid = -1;
                frameTable[i].page = NULL;*/
            }
        }//end for

        //check new process page table, see if any of its pages are mapped to frames.
        for (i = 0; i < vmStats.pages; i++){
            if (processes[new].pageTable[i].state == INFRAME){

                if (DEBUG){
                    USLOSS_Console("p1_switch(): about to map page %d\n", i);
                }
                error1 = USLOSS_MmuMap(TAG, i, processes[new].pageTable[i].frame, USLOSS_MMU_PROT_RW);
                if (error1 != USLOSS_MMU_OK){
                    USLOSS_Console("p1_switch(): couldn't map MMU, status %d\n", error1);
                    abort();
                }
                /*
                frameTable[i].state = USED;
                frameTable[i].pid = new;
                frameTable[i].page = &processes[new].pageTable[i];*/
            }
        }
        //map them to the frames they belong to
        //more profit
    }
    }
    else{

    }

    //MmuMap/MmuUnmap will be used to remove the pages from the frames,
    //and add the new pages into the frame
} /* p1_switch */

void
p1_quit(int pid)
{
    if (DEBUG)
        USLOSS_Console("p1_quit() called: pid = %d\n", pid);
} /* p1_quit */
/*
int
check_io()
{
    int toReturn = 0;
    
    Going to need more work before this will compilels

    mailbox* MailBoxTable = getMboxTable();
    int i;

    for (i = 0; i < 7; i ++){
    	if (MailBoxTable[i].nextBlockedProc != NULL)
    	{
    		toReturn = 1;
    	}
    }
    
    return toReturn;
}*/
