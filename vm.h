/*
 * vm.h
 */


/*
 * All processes use the same tag.
 */
#define TAG 0

/*
 * Different states for a page/frame.
 */
#define UNUSED  500
#define INFRAME 501
#define USED    502
#define ONDISK  503
#define ZAPPED  -500
/* You'll probably want more states */


/*
 * Page table entry.
 */
typedef struct PTE {
    int  state;      // See above.
    int  frame;      // Frame that stores the page (if any). -1 if none.
    int  diskBlock;  // Disk block that stores the page (if any). -1 if none.
    int  pageNum;
    // Add more stuff here
} PTE;

/*
 * Frame table entry.
 */
typedef struct FTE {
    int state;
    PTE *page;       // address of the page stored in the frame (if any) -1 if none
    int pid;        // pid of the process who owns the page
    int dirty;
    int ref;
} FTE;

/*
 * Per-process information.
 */
typedef struct Process {
    int  numPages;   // Size of the page table.
    PTE  *pageTable; // The page table for the process.
    int  pid;
} Process;

Process processes[50];
FTE *frameTable;
int mmuInitialized;
int curRefBlock;
void *vmRegion;
/*
 * Information about page faults. This message is sent by the faulting
 * process to the pager to request that the fault be handled.
 */
typedef struct FaultMsg {
    int  pid;        // Process with the problem.
    void *addr;      // Address that caused the fault.
    int  replyMbox;  // Mailbox to send reply.
    int  offset;
    // Add more stuff here.
} FaultMsg;

#define CheckMode() assert(USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE)
