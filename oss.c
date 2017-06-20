#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#include "commons.h"

void parseArgs(int argc, char **argv);
void init();
void initQ();
void initSharedMemory();
int initSem(int semval, int ftokIndex, int numsems);
void sigHandler(int signo); 
void sendSignal(int signo);
pid_t r_wait(int *stat_loc);
void doProcess();
void processRequest();
int getAvailSlot();
int getProcCount();
void canFork();
void forkProcess(int index);
void monitorRequestList();
void cleanup();
void addRequest(int index,ull wait);
void removeRequestFromQueue();
struct Node* getFront();
void monitorReqQueue();
int getTotFreeFrames();
void checkForDead();
void freeChildMemory(int index); 
void pageout_daemon();
void swap_page(int index);
void doSwap(int index);
void updateTimer(ull nsec);
void printResults();
void drawLine(int, int, int, char);
void printPageTable(int);
void printMemoryTable();
void printTable();
int getAllocFrameCount();

int shmid,n,nop;
SystemShare *ssd;

Queue q;
struct Node *front=NULL,*rear=NULL;
ull toNextFork;
float throughput;
int done=0,minframes;
int main(int argc,char *argv[])
{	
	setlocale(LC_NUMERIC, "");
	alarm(ALARMTIME);
	minframes=TOTMEMSIZE/10;
	parseArgs(argc, argv);	
	init();
	doProcess();
	cleanup();
	//return 0;
}
void parseArgs(int argc, char **argv) 
{
	n = DEFAULTPROC;
	int x=0;
	if (argc == 2) 
	{
		if (isInt(argv[1])) 
		{
			x = atoi(argv[1]);
		}
	}	
	if(x>0)
		n = x;
}
void init() 
{
	srand(time(NULL));
	shmid = setSharedMemory();
	printf("SHMID=%d\n",shmid);
	initQ();
	initLog();
	printf("Log initialized\n");
	initSharedMemory();
	printf("Shared Memory initialized\n");
	setSigHandler(*sigHandler);

}
void initQ() 
{
	int i;
	for (i=0; i<TOTMEMSIZE; i++) 
	{
		q.frames[i] = q.ref[i] = q.dirty[i] = 0;
	}
	q.head = 0;
	printf("Q initialized\n");	
}

void initSharedMemory() 
{
	ssd->timer.sec = ssd->timer.nsec = 0;
	ssd->pSem = initSem(0, 13, n);
	int i;

	ssd->numReqs = 0;
	ssd->numFaults = 0;
	
	for (i=0; i<MAXPROC; i++) 
	{
		Request *req = &ssd->reqlist[i];
		PageTable *tab = &ssd->tables[i];
		req->type = req->addr = req->flag = 0;
		int j;
		for (j=0; j<MAXPAGES; j++) 
		{
			tab->pages[j] = -1;
		}
		tab->numPages = 0;
		ssd->procStatFlag[i] = 0;
	}
}

int initSem(int semval, int ftokIndex, int numsems) 
{
	int semid;
	key_t key = ftok("./oss.c", ftokIndex);
	errno = 0;
	printf("Key=%d\nnumsem=%d\n",key,numsems);
	if ((semid = semget(key, numsems, 0666 | IPC_CREAT)) == -1) 
	{
		perror("Failed to access sempahore");
		exit(0);
	}
	int i;
	for (i=0; i<=numsems; i++)
		semctl(semid, i, SETVAL, semval);
	return semid;

}
void sigHandler(int signo) 
{
	if (signo == SIGINT) 
	{
		writeLog("OSS received SIGINT - Exiting.\n");
		sendSignal(SIGUSR1);
		cleanup();		
	}
	else if (signo == SIGALRM) 
	{
		sendSignal(signo);
		writeLog("OSS - Out of time. Cleaning up and terminating.\n");
	}
	else if (signo == SIGUSR1)
		return;
	cleanup();
}
void sendSignal(int signo) 
{
	int i;
	for (i=0; i<MAXPROC; i++) 
	{
		if (ssd->procStatFlag[i] == 1) 
		{
			kill(ssd->child[i], signo);
			r_wait(NULL);
		}
	}		
}
pid_t r_wait(int *stat_loc) 
{
	int retval;
	while (((retval = wait(stat_loc)) == -1) && (errno == EINTR));
	return retval;
}
void doProcess()
{
	int availslot=getAvailSlot();
	forkProcess(availslot);
	toNextFork = mstons(rand() % PINTERVAL + 1);
	printf("To Next Fork=%d\n",toNextFork);
	int cnt=0;
	while(1)
	{
		canFork();
		monitorRequestList();
		updateTimer(150);
		monitorReqQueue();
		checkForDead();			
		//usleep(1);
	}
}
void forkProcess(int index) 
{
	int pid;
	ssd->procStatFlag[index] = 1;
	pid = fork();
	if ( pid == 0 ) 
	{
		char *args = malloc(getDigitCount(index));
		int x = sprintf(args, "%d", index);		
		execl("./UserProc", "UserProc", args, NULL); 
		perror("Failed to exec a user process.");
	}
	else if (pid == -1 ) 
	{
		perror("Failed to fork.");
		sleep(1);
	}
}
int getAvailSlot() 
{
	int i;
	for (i=0; i<MAXPROC; i++) 
	{
		if (ssd->procStatFlag[i] == 0) 
			return i;
	}
	return -1;
}
void canFork() 
{
	if (toNextFork == 0) 
	{	
		int proccnt=getProcCount();
		if ( proccnt< MAXPROC && nop<n) 
		{			
			toNextFork = mstons(rand() % PINTERVAL + 1);
			printf("To Next Fork=%d\t",toNextFork);
			nop++;
			if(nop==n)
				throughput=1000.0f/((float)stoms(ssd->timer.sec)+nstoms(ssd->timer.nsec))*n;
			printf("No Procs=%d\tTill Time=%f msec %\n",nop,(float)stoms(ssd->timer.sec)+nstoms(ssd->timer.nsec));
			forkProcess(getAvailSlot());						
		}
	}
}
int getProcCount() 
{
	int i;
	int pcnt = 0;
	for (i=0; i<MAXPROC; i++) 
	{
		if (ssd->procStatFlag[i] == 1)
			pcnt++;
	}
	return pcnt;
}
void monitorRequestList() 
{
	int i;
	for (i=0; i<MAXPROC; i++) 
	{
		if (ssd->reqlist[i].flag == 1) 
		{
			//printf("Before Processing Req=%d\n",i);
			processRequest(i);
			//printf("After Processing Req=%d\n",i);
		}
	}
}
void processRequest(int index) 
{
	Request* req = &ssd->reqlist[index];
	int virtPage = req->addr/1024; 	
	PageTable *pagetable = &ssd->tables[index]; 
	int reqSlot = pagetable->pages[virtPage];
	//printf("Req Slot=%d\n",reqSlot);
	if (reqSlot != -1) 
	{
		//page is in memory		
		q.ref[reqSlot] = 1;
		pagetable->valid[virtPage]=1;
		Frame *curframe=&q.framelist[reqSlot];
		curframe->frame=reqSlot;
		curframe->pagetableidx=index;
		curframe->pageidx=virtPage;
		if (req->type == WRITE) 
		{
			q.dirty[reqSlot] = 1;
		}
		updateTimer(10);
		req->flag = 0; 
		semSig(ssd->pSem, index); 
	}
	else 
	{
		//page is not in memory
		//printf("Before Adding Req=%d\n",index);
		addRequest(index,mstons(15));
		//printf("After Adding Req=%d\n",index);
		req->flag = 0; //reset the flag
	}
}
void checkForDead() 
{
	int i;
	for (i=0; i<MAXPROC; i++) 
	{
		if (ssd->procStatFlag[i] == 2) 
		{			
			r_wait(NULL); 
			ssd->procStatFlag[i] = 0; 
			freeChildMemory(i);
		}
	}
}
void freeChildMemory(int index) 
{
	PageTable* tab = &ssd->tables[index]; 
	int i;
	for (i=0; i<tab->numPages; i++) 
	{
		int frame = tab->pages[i]; 
		q.frames[frame] = 0;
		q.ref[frame] = 0;
		q.dirty[frame] = 0;
		tab->pages[i] = -1; 
	}

	tab->numPages=0;
	semctl(ssd->pSem, index, SETVAL, 0);
	ssd->reqlist[index].flag = 0;


}
void updateTimer(ull nsec) 
{
	//printf("Before Update Timer\n");
	ssd->timer.nsec+=nsec;
	if (ssd->timer.nsec >= 1000000000) 
	{ 
		ssd->timer.nsec -= 1000000000; 
		ssd->timer.sec++; 
	}	
	int i;
	int ct=0;
	struct Node *ptr=NULL;
	//printf("Before For front=%u\n",front);
	for (ptr=front;ptr!=NULL;ptr=ptr->next) 
	{		
		if (ptr->req>= 0 ) 
		{
			if (ptr->wait>=nsec) 
			{				
				ptr->wait-=nsec;
			}
			else if (ptr->wait!= 0) 
			{
				ptr->wait=0;
			}
		}
		ct++;
	}
	if (toNextFork >= nsec)
		toNextFork-=nsec;
	else
		toNextFork=0;
	//printf("After Update Timer\n");		
}

void cleanup()
{
	printTable();
	printResults();
	char cmd[100];
	char sid[15];
	int i;
	for (i=0; i<n; i++) 
	{
		semctl(ssd->pSem, i, IPC_RMID);
	}
	strcpy(cmd,"ipcrm -m ");
	strcat(cmd,Itoa(shmid,sid,10));
	printf("Commnad=%s\n",cmd);
	system(cmd);
	exit(0);						
}
void clear()
{
	printTable();
	printResults();
	char cmd[100];
	char sid[15];
	int i;
	for (i=0; i<n; i++) 
	{
		semctl(ssd->pSem, i, IPC_RMID);
	}
	strcpy(cmd,"ipcrm -m ");
	strcat(cmd,Itoa(shmid,sid,10));	
	system(cmd);							
}
//-------------------------------------Req-Q FUNCTIONS---------------------
struct Node * getNode(int index,ull wait)
{
	//printf("Get struct Node Start\n");
	struct Node *newnode=(struct Node*)malloc(sizeof(struct Node));
	newnode->req=index;
	newnode->wait=wait;
	//printf("Get struct Node End=%u\n",newnode);
	return newnode;
}
void addRequest(int index,ull wait)
{
	struct Node *newnode=getNode(index,wait);
	if(rear==NULL)
	{
		front=newnode;
		rear=newnode;
		//printf("First struct Node=%u\n",front);
		//newnode->next=NULL;		
		return;
	}
	rear->next=newnode;
	newnode->next=NULL;
	rear=newnode;		
}
void removeRequestFromQueue()
{
	if(front==NULL)
	{
		debugLog("Request Queue is Empty\n");
		return;
	}
	struct Node *ptr=front;
	front=front->next;
	if(front==NULL)
		rear=NULL;
	free(ptr);
}
struct Node* getFront()
{
	if(rear==NULL||front==NULL)
	{
		//debugLog("Request Queue is Empty\n");
		return NULL;
	}
	return front;
}
void monitorReqQueue()
{
	struct Node *curhead=getFront();
	if(curhead!=NULL)
	{
		int index=curhead->req;
		if(curhead->wait<=0)
		{
			swap_page(index);
			ssd->reqlist[index].flag = 0; //reset the flag
			semSig(ssd->pSem, index); //signal on the semaphore
			removeRequestFromQueue();
		}		
	}
}


int getTotFreeFrames()
{
	int freecnt=0,i;
	for(i=0;i<TOTMEMSIZE;i++)
	{
		if(q.frames[i]==0||q.frames[i]==2)
			freecnt++;
	}
	return freecnt;
}

void pageout_daemon()
{
	int i,totfree;
	totfree=getTotFreeFrames();
	fprintf(stderr,"Page Out Deamon: Total Free=%d\tMinimum Frame Limit=%d\n",totfree,minframes);
	if(totfree<minframes)
	{
		for(i=0;i<TOTMEMSIZE;i++)
		{
			if(q.frames[i]==1)
			{
				Frame *curframe=&q.framelist[i];
				PageTable *pt=&ssd->tables[curframe->pagetableidx];
				if(pt->valid[curframe->pageidx]==0)
				{
					q.frames[i]=0;
					q.ref[i]=0;
				}
				else
				{
					pt->valid[curframe->pageidx]=0;
					q.frames[i]=2;										
				}
				
			}
		}
	}
}


void swap_page(int index) 
{
	int totfree=getTotFreeFrames();
	//fprintf(stderr,"Page Swap Total Free=%d\n",totfree);
	if(totfree<minframes)	
	{
		pageout_daemon();
	}
	while(1)
	{			
		if (q.ref[q.head] == 1) 
		{			
			q.head = ++q.head%TOTMEMSIZE;
		}
		else 
		{ 			
			doSwap(index);
			//printf("Do Swap=%d\n",index);
			q.head = ++q.head%TOTMEMSIZE;
			break;
		}
		
	}
}

void doSwap(int index) 
{
	int i;
	for (i=0; i<MAXPROC; i++) 
	{
		PageTable* table = &ssd->tables[i]; 
		int j;
		for (j=0; j<table->numPages; j++) 
		{ 
			if (table->pages[j] == q.head) 
			{
				table->pages[j] = -1; 
				if (q.dirty[q.head] == 1)
				{ 
					updateTimer(mstons(15)); 
				}
			}
		}
	}
	PageTable* table = &ssd->tables[index]; //pull the process who's request we're granting's table
	Request* req = &ssd->reqlist[index];
	int virtPage = req->addr/1024; //get the virtual page that this process is wanting
	//debugLog("\n\n\nGranting request for p%d addr %d page %d.\n\n\n\n", index, req->addr, virtPage);
	table->pages[virtPage] = q.head;
	q.frames[q.head] = 1;
	Frame *curframe=&q.framelist[q.head];
	curframe->frame=q.head;
	curframe->pagetableidx=index;
	curframe->pageidx=virtPage;
	q.ref[q.head] = 1;
	q.dirty[q.head] = 0;
	ssd->reqlist[index].flag = 0; 

}

int getAllocFrameCount() 
{
	int ct = 0;
	int i;
	for (i=0; i<TOTMEMSIZE; i++) 
	{
		if (q.frames[i] == 1)
			ct++;
	}
	return ct;
}
void printResults() 
{
	ull avgAccessNS = (((ssd->numReqs-ssd->numFaults)*10) + (ssd->numFaults * mstons(15))) / ssd->numReqs;
	float avgAccessMS = nstoms(avgAccessNS);
	
	double faultRt = ((float)ssd->numFaults / (float)ssd->numReqs) * 100.0;
	writeLog("=======================================\n");
	writeLog("FINAL REPORT\n");
	writeLog("REQUESTS    : %d\n", ssd->numReqs);
	writeLog("PAGE FAULTS : %d\n", ssd->numFaults);
	writeLog("FAULT RATE  : %.2f%%\n", faultRt);
	writeLog("THROUGHPUT  : %.2f\n", throughput);
	if (avgAccessMS >= 1.0)
		writeLog("AVG ACCESS  : %.2fms\n", avgAccessMS);
	else
		writeLog("AVG ACCESS  : %lluns\n", avgAccessNS);
	writeLog("=======================================\n\n");
}

void drawLine(int length, int offset, int extra, char c) 
{
	int i, j;
	for (i=0; i<length; i++) 
	{
		for (j=0; j<=offset; j++)
			writeLog("%c", c);
	}
	for (i=0; i<extra; i++)
		writeLog("%c", c);
	writeLog("\n");
}

void printPageTable(int index) 
{
	drawLine(32, 4, 10, '=');
	PageTable* table = &ssd->tables[index]; 
	writeLog("Process %d Page Table\n", index);
	int printTimes = MAXPAGES / 32;
	if (MAXPAGES % 32 > 0)
		printTimes++;
	int left = table->numPages;
	int i;
	int numPrint = 32;
	int initJ = 0;
	int lineSize;
	for (i=0; i<printTimes; i++) 
	{
		int j;
		int extra=10;
		if (left > 32)
			lineSize = 32;
		else
			lineSize = left;

		drawLine(lineSize, 4, extra, '-');
		writeLog("|Logical ");
		for (j=0; j<lineSize; j++ ) 
		{
			writeLog("|%4d", j+(32*i));

		}
		writeLog("|\n");
		drawLine(lineSize, 4, extra, '-');
		writeLog("|Physical");
		for (j=initJ; j<lineSize; j++) 
		{
			if (table->pages[j] != -1)
				writeLog("|%4d", table->pages[j]);
			else
				writeLog("|    ");
		}		
		writeLog("|\n");
		drawLine(lineSize, 4, extra, '-');
		writeLog("\n");
		left-=lineSize;
		if (left<=0)
			break;
	}

}


void printMemoryTable() 
{
	int printMax = getAllocFrameCount();
	printf("Allocated Frame Count=%d\n",printMax);
	int printCt = 0;
	int numCols = 32;
	int numPrints = TOTMEMSIZE/numCols;
	if (TOTMEMSIZE % numCols > 0)
		numPrints++;
	int i, j;
	int extra = 8;
	writeLog("Memory Allocation Table\n");
	for (i=0; i<numPrints; i++) 
	{ //have to print numPrints time
		if (printCt == printMax)
			break;
		drawLine(numCols, 4, extra, '-');
		writeLog("|Frame ");
		for (j=0; j<numCols; j++) 
		{
			int n = j+(numCols * i);
			if (n==TOTMEMSIZE)
				break;
			//if (q.frames[n] == 1)
				writeLog("|%4d", n); //print the header
		}
		writeLog("|\n");
		drawLine(numCols, 4, extra, '-');
		writeLog("|Alloc ");
		for (j=0; j<numCols; j++) 
		{
			int n = j+(numCols * i);
			if (n==TOTMEMSIZE)
				break;
			if (q.frames[n] == 1)
				writeLog("|%4d", q.frames[n]);
			else
				writeLog("|    ");
				printCt++;
			//}
		}
		writeLog("|\n");
		drawLine(numCols, 4, extra, '-');
		writeLog("\n");

	}
}
void printTable() 
{
	int i;
	printMemoryTable();
	for (i=0; i<n; i++) 
	{
		if (ssd->procStatFlag[i] > 0)
			printPageTable(i);
	}
	drawLine(32, 4, 10, '=');

}



