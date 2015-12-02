
#include "usloss.h"
#include <vm.h>
#include <phase1.h>
#include "phase2.h" 
#include <phase3.h>
#include <phase4.h>
#include <phase5.h>
#include <stdio.h>
#include <string.h>

#define DEBUG 1

extern int debugflag;
extern Process processes[50];
extern int mmuInitialized;

void
p1_fork(int pid)
{
    //this prevents the earlier phases from calling this before init is called
    if(mmuInitialized){
        if (DEBUG)
            USLOSS_Console("p1_fork() called: pid = %d, MMU online\n", pid);

        //initialize process table entry
        processes[pid%MAXPROC].pid = pid;
        processes[pid%MAXPROC].numPages = vmStats.pages;
        processes[pid%MAXPROC].pageTable = malloc(USLOSS_MmuPageSize()*vmStats.pages);

        //initialize page table
        int i;
        for (i = 0; i < vmStats.pages; i++){
            processes[pid%MAXPROC].pageTable[i].state = UNUSED;
            processes[pid%MAXPROC].pageTable[i].frame = -1;
            processes[pid%MAXPROC].pageTable[i].diskBlock = -1;
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
