/*
 * Function prototypes from Patrick's phase3 solution. These can be called
 * when in *kernel* mode to get access to phase3 functionality.
 */


#ifndef PROVIDED_PROTOTYPES_H

#define PROVIDED_PROTOTYPES_H

extern int  spawnReal(char *name, int (*func)(char *), char *arg,
                       int stack_size, int priority);
extern int  waitReal(int *status);
extern void terminateReal(int exit_code);
extern int  semcreateReal(int init_value);
extern int  sempReal(int semaphore);
extern int  semvReal(int semaphore);
extern int  semfreeReal(int semaphore);
extern int  gettimeofdayReal(int *time);
extern int  cputimeReal(int *time);
extern int  getPID_real(int *pid);

extern int  diskReadReal (int unit, int track, int first_sector,
                          int numSectors, void *buffer);
extern int  diskWriteReal(int unit, int track, int first_sector,
                          int numSectors, void *buffer);

extern int  MboxCreate(int numslots, int slotsize);
extern int  MboxRelease(int mboxID);
extern int  MboxSend(int mboxID, void *msgPtr, int msgSize);
extern int  MboxReceive(int mboxID, void *msgPtr, int msgSize);
extern int  MboxCondSend(int mboxID, void *msgPtr, int msgSize);
extern int  MboxCondReceive(int mboxID, void *msgPtr, int msgSize);

#endif  /* PROVIDED_PROTOTYPES_H */
