/*
 * skeleton.c
 *
 * This is a skeleton for phase5 of the programming assignment. It
 * doesn't do much -- it is just intended to get you started.
 */


#include <assert.h>
#include <phase1.h>
#include "phase2.h" 
#include <phase3.h>
#include <phase4.h>
#include <phase5.h>
#include <usyscall.h>
#include <libuser.h>
#include <vm.h>
#include <string.h>
#include <usyscall.h>
#include <usloss.h>
#include <stdio.h>
#include <stdlib.h>
#include <provided_prototypes.h>


FaultMsg faults[MAXPROC]; /* Note that a process can have only
                           * one fault at a time, so we can
                           * allocate the messages statically
                           * and index them by pid. */
VmStats  vmStats;



//extern Process processes[50];
extern int mmuInitialized;
int pagerMbox;
int pagerPID[MAXPAGERS];
int numPagers = 0;

int debug5 = 1;


static void
FaultHandler(int  type,  // USLOSS_MMU_INT
             void *arg); // Offset within VM region

static void vmInit(systemArgs *sysargs);
static void vmDestroy(systemArgs *sysargs);
static int Pager(char *buf);
static int PagerClock();

/*
 *----------------------------------------------------------------------
 *
 * start4 --
 *
 * Initializes the VM system call handlers. 
 *
 * Results:
 *      MMU return status
 *
 * Side effects:
 *      The MMU is initialized.
 *
 *----------------------------------------------------------------------
 */
int
start4(char *arg)
{
    int pid;
    int result;
    int status;
    mmuInitialized = 0;
    curRefBlock = 0;

    if(debug5){
      USLOSS_Console("start4(): Started.\n");
    }

    /* to get user-process access to mailbox functions */
    systemCallVec[SYS_MBOXCREATE]      = (void*)MboxCreate;
    systemCallVec[SYS_MBOXRELEASE]     = (void*)MboxRelease;
    systemCallVec[SYS_MBOXSEND]        = (void*)MboxSend;
    systemCallVec[SYS_MBOXRECEIVE]     = (void*)MboxReceive;
    systemCallVec[SYS_MBOXCONDSEND]    = (void*)MboxCondSend;
    systemCallVec[SYS_MBOXCONDRECEIVE] = (void*)MboxCondReceive;

    /* user-process access to VM functions */
    systemCallVec[SYS_VMINIT]    = vmInit;
    systemCallVec[SYS_VMDESTROY] = vmDestroy;

    //initialize Phase 5 Process Table
    int i;
    for (i = 0; i < MAXPROC; i++){
      processes[i].numPages = -1;
      processes[i].pid = -1;
      processes[i].pageTable = NULL;
    }

    result = Spawn("Start5", start5, NULL, 8*USLOSS_MIN_STACK, 2, &pid);
    if (result != 0) {
        USLOSS_Console("start4(): Error spawning start5\n");
        Terminate(1);
    }
    result = Wait(&pid, &status);
    if (result != 0) {
        USLOSS_Console("start4(): Error waiting for start5\n");
        Terminate(1);
    }
    Terminate(0);
    return 0; // not reached

} /* start4 */

/*
 *----------------------------------------------------------------------
 *
 * VmInit --
 *
 * Stub for the VmInit system call.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      VM system is initialized.
 *
 *----------------------------------------------------------------------
 */
static void
vmInit(systemArgs *sysargs)
{
    CheckMode();

    //extract arguments
    int mappings = (int)sysargs->arg1;
    int pages    = (int)sysargs->arg2;
    int frames   = (int)sysargs->arg3;
    int pagers   = (int)sysargs->arg4;

    //check for illegal values
    if (mappings != pages || pages <= 0 || frames <= 0 || pagers <= 0){
      USLOSS_Console("vmInit(): Illegal values given as input!\n");
      sysargs->arg4 = (void *) (long) -1;
      return;
    }
    else{
      sysargs->arg4 = (void *) (long) 0;
    }

    vmRegion = vmInitReal(mappings, pages, frames, pagers);

    //set arg1 to the address of vmRegion
    sysargs->arg1 = vmRegion;
    return;
} /* vmInit */


/*
 *----------------------------------------------------------------------
 *
 * vmInitReal --
 *
 * Called by vmInit.
 * Initializes the VM system by configuring the MMU and setting
 * up the page tables.
 *
 * Results:
 *      Address of the VM region.
 *
 * Side effects:
 *      The MMU is initialized.
 *
 *----------------------------------------------------------------------
 */
void *
vmInitReal(int mappings, int pages, int frames, int pagers)
{
   int status;
   int dummy, dummy2;
   numPagers = pagers;
   mmuInitialized = 1;

   CheckMode();

   if(debug5){
      USLOSS_Console("vmInitReal(): called.\n");
    }

   status = USLOSS_MmuInit(mappings, pages, frames);
   if (status != USLOSS_MMU_OK) {
      USLOSS_Console("vmInitReal: couldn't init MMU, status %d\n", status);
      abort();
   }
   USLOSS_IntVec[USLOSS_MMU_INT] = FaultHandler;

   /*
    * Initialize frame table
    */
   frameTable = malloc(sizeof(FTE)*frames);
   int i;
   for(i = 0; i < frames; i++){
      frameTable[i].state = UNUSED;
      frameTable[i].page = NULL;
      frameTable[i].pid = -1;
      frameTable[i].dirty = -1;
      frameTable[i].ref = -1;
   }
  
   /* 
    * Create the fault mailbox.
    */
   
   for (i = 0; i < MAXPROC; i++){
      faults[i].pid = -1;
      faults[i].addr = NULL;
      faults[i].replyMbox = -1;
   }


   /*
    * Fork the pagers.
    */

   //create mailbox for pagers to block on
   pagerMbox = MboxCreate(pagers, sizeof(FaultMsg*));
   char buf[10];
   for (i = 0; i < pagers; i++){
      sprintf(buf, "%d", i);
      pagerPID[i] = fork1("Pager", Pager, buf, USLOSS_MIN_STACK, 2);
   }
   for (i = pagers; i < MAXPAGERS; i++){
      pagerPID[i] = -1;
   }


   /*
    * Zero out, then initialize, the vmStats structure
    */
   memset((char *) &vmStats, 0, sizeof(VmStats));
   vmStats.pages = pages;
   vmStats.frames = frames;
   diskSizeReal(1, &dummy2, &dummy2, &dummy);
   vmStats.diskBlocks = dummy;
   vmStats.freeFrames = frames;
   vmStats.freeDiskBlocks = dummy;
   vmStats.switches = 0;
   vmStats.faults = 0;
   vmStats.new = 0;
   vmStats.pageIns = 0;
   vmStats.pageOuts = 0;
   vmStats.replaced = 0;

   vmRegion = USLOSS_MmuRegion(&dummy);

   if(debug5){
      USLOSS_Console("vmInitReal(): diskBlocks = %d. Num pages: %d PageSize = %d frames = %d\n", vmStats.diskBlocks, dummy, USLOSS_MmuPageSize(), vmStats.frames);

   }

   return USLOSS_MmuRegion(&dummy);
} /* vmInitReal */

/*
 *----------------------------------------------------------------------
 *
 * vmDestroy --
 *
 * Stub for the VmDestroy system call.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      VM system is cleaned up.
 *
 *----------------------------------------------------------------------
 */
static void
vmDestroy(systemArgs *sysargs)
{
   CheckMode();
   
	if(mmuInitialized != 1)
		return;
   
   
	vmDestroyReal();
   
} /* vmDestroy */

/*
 *----------------------------------------------------------------------
 *
 * vmDestroyReal --
 *
 * Called by vmDestroy.
 * Frees all of the global data structures
 *
 * Results:
 *      None
 *
 * Side effects:
 *      The MMU is turned off.
 *
 *----------------------------------------------------------------------
 */
void
vmDestroyReal(void)
{
	int status;
	CheckMode();

  mmuInitialized = 0;

	/*
	* Kill the pagers here.
    */

	int i;
	for (i = 0; i < numPagers; i++){
		int msg = ZAPPED;
		MboxSend(pagerMbox, &msg, sizeof(msg));
		join(&status);
	}
	if(debug5){
		USLOSS_Console("vmDestroyReal(): pagers quit.\n");
	}

	//free frame table
	free(frameTable);
	
   /* 
    * Print vm statistics.
    */
	PrintStats();
   /* and so on... */

	USLOSS_MmuDone();
	if(debug5){
		USLOSS_Console("vmDestroyReal(): MMU destroyed.\n");
	}

	

} /* vmDestroyReal */


/*
 *----------------------------------------------------------------------
 *
 * PrintStats --
 *
 *      Print out VM statistics.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Stuff is printed to the USLOSS_Console.
 *
 *----------------------------------------------------------------------
 */
void
PrintStats(void)
{
     USLOSS_Console("VmStats\n");
     USLOSS_Console("pages:          %d\n", vmStats.pages);
     USLOSS_Console("frames:         %d\n", vmStats.frames);
     USLOSS_Console("diskBlocks:     %d\n", vmStats.diskBlocks);
     USLOSS_Console("freeFrames:     %d\n", vmStats.freeFrames);
     USLOSS_Console("freeDiskBlocks: %d\n", vmStats.freeDiskBlocks);
     USLOSS_Console("switches:       %d\n", vmStats.switches);
     USLOSS_Console("faults:         %d\n", vmStats.faults);
     USLOSS_Console("new:            %d\n", vmStats.new);
     USLOSS_Console("pageIns:        %d\n", vmStats.pageIns);
     USLOSS_Console("pageOuts:       %d\n", vmStats.pageOuts);
     USLOSS_Console("replaced:       %d\n", vmStats.replaced);
} /* PrintStats */


/*
 *----------------------------------------------------------------------
 *
 * FaultHandler
 *
 * Handles an MMU interrupt. Simply stores information about the
 * fault in a queue, wakes a waiting pager, and blocks until
 * the fault has been handled.
 *
 * Results:
 * None.
 *
 * Side effects:
 * The current process is blocked until the fault is handled.
 *
 *----------------------------------------------------------------------
 */
static void
FaultHandler(int  type /* USLOSS_MMU_INT */,
             void *arg  /* Integer, Offset within VM region */)
{
   int cause;

   int offset = (int) (long) arg;

   assert(type == USLOSS_MMU_INT);
   cause = USLOSS_MmuGetCause();
   assert(cause == USLOSS_MMU_FAULT);
   vmStats.faults++;
   /*
    * Fill in faults[pid % MAXPROC], send it to the pagers, and wait for the
    * reply.
    */

   int pid = 0;
   getPID_real(&pid);

   faults[pid%MAXPROC].pid = pid;
   faults[pid%MAXPROC].addr = vmRegion + offset;
   faults[pid%MAXPROC].replyMbox = MboxCreate(0, 0);
   faults[pid%MAXPROC].offset = offset;
   if(debug5){
      USLOSS_Console("FaultHandler(): sending fault to pagerMbox, offset = %d addr = %p\n", offset, vmRegion + offset);
   }

   int msg = pid;
   MboxSend(pagerMbox, &msg, sizeof(msg));
   //block on private mbox
   MboxReceive(faults[pid%MAXPROC].replyMbox, NULL, 0);
   if(debug5){
      USLOSS_Console("FaultHandler(): Pager sent reply, page is ready\n");
   }

} /* FaultHandler */

/*
 *----------------------------------------------------------------------
 *
 * Pager 
 *
 * Kernel process that handles page faults and does page replacement.
 *
 * Results:
 * None.
 *
 * Side effects:
 * None.
 *
 *----------------------------------------------------------------------
 */
static int
Pager(char *buf)
{
  char* num = buf;
  if(debug5){
    USLOSS_Console("Pager%c(): started\n", buf[0]);
  }
  int pid = -1;
  int error = 0;
  int offset;
  int status;
  getPID_real(&pid);
  int frameToMap;
  int pageToMap;

  //enable interrupts
  USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);

  while(1) {

      /* Wait for fault to occur (receive from mailbox) */
      int pidToHelp = -1;
      MboxReceive(pagerMbox, &pidToHelp, sizeof(int));

      //check to see if the mail is a zap from start4
      if (pidToHelp == ZAPPED){
        if(debug5)
          USLOSS_Console("Pager%c(): Zapped! Quitting\n", buf[0]);
        quit(1);
        return 0;
      }

      //Look for usable frame in frame table
      int i;
      for (i = 0; i < vmStats.frames; i++){
          if(frameTable[i].state == UNUSED){
            break;
          }
		      else if (i == vmStats.frames-1 && frameTable[i].state == USED){		  
			      //Call PagerClock
			      i = PagerClock(i);
			      break;
		      }
      }
      frameToMap = i;

      //look for free page in procs page table
      int j;
      for (j = 0; i<vmStats.pages; j++){
        if(processes[pidToHelp].pageTable[j].state == UNUSED)
          break;
      }
      pageToMap = j;


      if(debug5)
          USLOSS_Console("Pager%c(): Frame %d and Page %d are free free\n\t  page currently in frame: dirty: %d, ref: %d\n", buf[0], frameToMap, pageToMap, frameTable[i].dirty, frameTable[i].ref);

      offset = faults[pidToHelp].offset;

      //if it's dirty, write it to the disk
      if (frameTable[i].dirty == USLOSS_MMU_DIRTY){

        char toWrite [USLOSS_MmuPageSize() + 1];
        int toUnmap = frameTable[frameToMap].page->pageNum;

        if(debug5)
          USLOSS_Console("Pager(): dirty frame! Saving to disk\n");

        //map pagers own page onto the frame to get access
        error = USLOSS_MmuMap(TAG, toUnmap, frameToMap, USLOSS_MMU_PROT_RW);
        if (error != USLOSS_MMU_OK){
          USLOSS_Console("Pager(): couldn't map page %d frame %d in MMU, status %d\n", toUnmap, frameToMap, error);
          abort();
        }

        memcpy(toWrite, vmRegion, USLOSS_MmuPageSize());

        //write to disk

        error = USLOSS_MmuUnmap(TAG, toUnmap);
        if (error != USLOSS_MMU_OK){
          USLOSS_Console("Pager(): couldn't unmap MMU, status %d\n", error);
          abort();
        }


      }

      //map the page j to the open frame i
      error = USLOSS_MmuMap(TAG, pageToMap, frameToMap, USLOSS_MMU_PROT_RW);
      if (error != USLOSS_MMU_OK){
		    USLOSS_Console("Pager(): couldn't map MMU, status %d\n", error);
        abort();
      }
      if(debug5)
        USLOSS_Console("Pager(): mapped to frame %d\n", frameToMap);

      //update the process page table
      vmStats.new++;
      processes[pidToHelp].pageTable[pageToMap].state = INFRAME;
      processes[pidToHelp].pageTable[pageToMap].frame = frameToMap;

      //clear the new page
      memset(vmRegion+(pageToMap*USLOSS_MmuPageSize()), 0, USLOSS_MmuPageSize());

      error = USLOSS_MmuUnmap(TAG, pageToMap);
        if (error != USLOSS_MMU_OK){
          USLOSS_Console("Pager(): couldn't unmap MMU, status %d\n", error);
          abort();
        }
    
      //update the frame table
      //frameTable[frameToMap].state = USED;
      //frameTable[frameToMap].page = &processes[pidToHelp].pageTable[pageToMap];
      //frameTable[frameToMap].pid = pidToHelp;

      
      /* Load page into frame from disk, if necessary */
      /* Unblock waiting (faulting) process */

      MboxSend(faults[pidToHelp%MAXPROC].replyMbox, NULL, 0);
    }
    return 0;
} /* Pager */

static int
PagerClock(int cur)
{
	int freeFrame = 0;
	int error;
	int accessPtr;
	//find frame to replace
	
	if(debug5){
		USLOSS_Console("PagerClock() called.\nFrames= %d\n", vmStats.frames);
	}
	
	
	if(cur >= vmStats.frames-1){ //Subtract 1 to account for 0 indexing
		freeFrame = cur % (vmStats.frames-1);
		if(debug5)
			USLOSS_Console("PagerClock(): freeFrame = %d\n", freeFrame);
	}
	
	int i;
  int lastBestFrame = 0;
  int lastBestPriority = 5;
	
	for(i=0; i<vmStats.frames; i++){
    accessPtr = 0;
		error = USLOSS_MmuGetAccess(i, &accessPtr);
		if(error != USLOSS_MMU_OK)
			USLOSS_Console("PagerClock(): Couldn't get accessPtr, status %d\n", error);
			
		if( ((accessPtr&USLOSS_MMU_REF) != USLOSS_MMU_REF) && ((accessPtr&USLOSS_MMU_DIRTY) != USLOSS_MMU_DIRTY)
      && lastBestPriority > 1){
		 //First priority to switch
      lastBestPriority = 1;
      lastBestFrame = i;
		}
		else if( ((accessPtr&USLOSS_MMU_REF) != USLOSS_MMU_REF) && ((accessPtr&USLOSS_MMU_DIRTY) == USLOSS_MMU_DIRTY) 
      && lastBestPriority > 2){
			//Second priority
      lastBestPriority = 2;
      lastBestFrame = i;
		}
		else if( ((accessPtr&USLOSS_MMU_REF) == USLOSS_MMU_REF) && ((accessPtr&USLOSS_MMU_DIRTY) != USLOSS_MMU_DIRTY) 
      && lastBestPriority > 3){
			//Third
      lastBestPriority = 3;
      lastBestFrame = i;
		}
		else if (lastBestPriority > 4){
			lastBestPriority = 4;
      lastBestFrame = i;
		}
	
	}
  accessPtr = 0;

  freeFrame = lastBestFrame;
  error = USLOSS_MmuGetAccess(freeFrame, &accessPtr);
  frameTable[freeFrame].dirty = accessPtr&USLOSS_MMU_DIRTY;
  frameTable[i].ref = accessPtr&USLOSS_MMU_REF;
  if(error != USLOSS_MMU_OK)
    USLOSS_Console("PagerClock(): Couldn't get accessPtr, status %d\n", error);
	
	
	//send free frame # back to Pager
	return freeFrame;
}