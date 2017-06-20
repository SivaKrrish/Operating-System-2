#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <locale.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <wait.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/shm.h>

#define MAXPROC 18
#define DEFAULTPROC 12
#define TOTMEMSIZE 256
#define MAXPAGES 512
#define ALARMTIME 60
#define PINTERVAL 20
#define DEBUG 1


typedef unsigned long long ull;
typedef unsigned int ui;

typedef enum {READ, WRITE} reqType;
union semun 
{
	int val;
	struct semid_ds *buf;
	ushort *array;
};
typedef struct
{
	ui sec;
	ui nsec;
}Timer;
typedef struct 
{
	int pages[MAXPAGES];
	int valid[MAXPAGES];
	int numPages;
}PageTable;
typedef struct 
{
	reqType type; //the type of the request
	int addr; //the address fo the request;
	int flag; //request pending
}Request;
typedef struct
{
	int frame;
	int pagetableidx;
	int pageidx;
}Frame;
typedef struct 
{
	int head;
	int frames[TOTMEMSIZE]; //each individual frame slot, free=0,alloted=1,mark for replacement=2
	Frame framelist[TOTMEMSIZE];// track for virtual pages
	int ref[TOTMEMSIZE]; //recently referenced?
	int dirty[TOTMEMSIZE]; //the dirty bit
}Queue;
/*typedef struct 
{
	int head;
	ull wait[RQSIZE]; //how long to wait
	int req[RQSIZE];
}ReqQueue;*/

struct Node
{	
	ull wait; //how long to wait
	int req;
	struct Node *next;
};

typedef struct 
{
	Timer timer; //the simulated clock.
	int pSem; //semaphores for each process, to be signaled on when their request is fulfilled.
	int numReqs;
	int numFaults; //overall count of requests and faults.

	PageTable tables[MAXPROC];
	Request reqlist[MAXPROC];
	int procStatFlag[MAXPROC]; //flags for processes. 1 means they exist, 2 means they need to be waited on.
	pid_t child[MAXPROC];
} SystemShare;

int setSharedMemory();
void semWait(int, int);
void semSig(int, int);
void setSigHandler( void handler() );

void writeLog (const char * format, ...);
void debugLog (const char * format, ...);

float nstoms(ull);
ull mstons(float ms);
float nstosec(ull);
ull sectons(int s);

int isInt(char *str);
int getDigitCount(int num);
char* Itoa(int value, char* str, int radix);




