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

FTE *frameTable;

//extern Process processes[50];
extern int mmuInitialized;
int pagerMbox;
int pagerPID[MAXPAGERS];

void *vmRegion;
int debug5 = 1;

static void
FaultHandler(int  type,  // USLOSS_MMU_INT
             void *arg); // Offset within VM region

static void vmInit(systemArgs *sysargs);
static void vmDestroy(systemArgs *sysargs);
static int Pager(char *buf);
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
    sysargs->arg1 = (void*) vmRegion;
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

   //vmRegion = malloc( USLOSS_MmuPageSize() * pages );

   if(debug5){
      USLOSS_Console("vmInitReal(): diskBlocks = %d. Num pages: %d\n", vmStats.diskBlocks, dummy);
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
   
	int mappings = (int)sysargs->arg1;
	int pages 	= (int)sysargs->arg2;
	int frames	= (int)sysargs->arg3;
	int pagers	= (int)sysargs->arg4;
   
   
	vmDestroyReal(mappings, pages, frames, pagers);
   
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
vmDestroyReal(int mappings, int pages, int frames, int pagers)
{
	int status;
	CheckMode();

	USLOSS_MmuDone();
	
	/*
	* Kill the pagers here.
    */
	int i;
	for (i = 0; i < pagers; i++){
		status = zap(pagerPID[i]); //I think?
	}
	
	free(frameTable);
	
   /* 
    * Print vm statistics.
    */
	PrintStats();
   /* and so on... */

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
  getPID_real(&pid);
  //enable interrupts
  USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
  while(1) {
      /* Wait for fault to occur (receive from mailbox) */
      int pidToHelp = -1;
      MboxReceive(pagerMbox, &pidToHelp, sizeof(int));
      //Look for free frame 
      int i;
      for (i = 0; i < vmStats.frames; i++){
          if(frameTable[i].state == UNUSED){
            break;
          }
      }
      /* If there isn't one then use clock algorithm to
       * replace a page (perhaps write to disk) 
         For now, lets just assume there always is one*/
      offset = faults[pidToHelp].offset;
      //if the page hasn't been accessed before, just set it up
      if (processes[pidToHelp].pageTable[offset].state == UNUSED){
        error = USLOSS_MmuMap(TAG, offset, i, USLOSS_MMU_PROT_RW);
        if (error != USLOSS_MMU_OK){
          USLOSS_Console("Pager(): couldn't map MMU, status %d\n", error);
          abort();
        }
        if(debug5)
          USLOSS_Console("Pager(): mapped to frame %d\n", i);
        vmStats.new++;
        processes[pidToHelp].pageTable[offset].state = INFRAME;
        processes[pidToHelp].pageTable[offset].frame = i;

        memset(vmRegion+offset, 0, USLOSS_MmuPageSize());


      }

      //update the frame table
      frameTable[i].state = USED;
      frameTable[i].page = &processes[pidToHelp].pageTable[offset];
      frameTable[i].pid = pidToHelp;

      
      /* Load page into frame from disk, if necessary */
      /* Unblock waiting (faulting) process */

      MboxSend(faults[pidToHelp%MAXPROC].replyMbox, NULL, 0);
    }
    return 0;
} /* Pager */
