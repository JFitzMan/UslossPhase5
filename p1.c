
#include "usloss.h"
//#include "message.h"
#include "phase2.h" 
#include <stdio.h>
#include <string.h>

#define DEBUG 0
extern int debugflag;

void
p1_fork(int pid)
{
    if (DEBUG && debugflag)
        USLOSS_Console("p1_fork() called: pid = %d\n", pid);
} /* p1_fork */

void
p1_switch(int old, int new)
{
    if (DEBUG && debugflag)
        USLOSS_Console("p1_switch() called: old = %d, new = %d\n", old, new);
} /* p1_switch */

void
p1_quit(int pid)
{
    if (DEBUG && debugflag)
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
