#include "commons.h"

void doProcess();
void genRequest();
void cleanup();
void sigHandler(int);


SystemShare *ssd;
int curIndex;
PageTable* curTable;
int numFaults;
int numReqs;
int maxAddr;
Timer thisAge;
double totwt;
int main(int argc, char **argv) 
{
	signal(SIGINT, SIG_IGN);
	setlocale(LC_NUMERIC, "");
	setSharedMemory();
	setSigHandler(*sigHandler);
	curIndex = atoi(argv[1]);
	curTable = &ssd->tables[curIndex];
	printf("Generated process=%d\tIndex=%d.\n", getpid(),curIndex);

	srand(time(NULL)+curIndex);
	curTable->numPages = rand() % MAXPAGES + 1;
	maxAddr = curTable->numPages*1024;

	thisAge.sec = ssd->timer.sec;
	thisAge.nsec = ssd->timer.nsec;

	ssd->procStatFlag[curIndex] = 1;
	ssd->child[curIndex] = getpid();
	
	doProcess();
	
	return 0;
}


void doProcess() 
{
	int numReqs = rand()%100 + 1000;
	int i = 0;
	for (i=0;i<numReqs;i++) 
	{
		clock_t tic = clock();
		genRequest();
				
		semWait(ssd->pSem, curIndex);	
		clock_t toc = clock();
		double curwt=(double)(toc - tic) / CLOCKS_PER_SEC;
		totwt+=curwt;	
		
		if (i==numReqs-1) 
		{
			if (rand() % 6 != 1) 
			{
				i=0;
				numReqs = rand()%100 + 1000;
			}
		}
		
	}
	cleanup();
}



void genRequest() 
{
	reqType type;
	int rw = rand() % 2;
	if (rw == 1)
		type = WRITE;
	else
		type=READ;

	int addr = rand()%(maxAddr+1);
	ssd->reqlist[curIndex].type = type;
	ssd->reqlist[curIndex].addr = addr;
	ssd->reqlist[curIndex].flag = 1;

	int page = addr/1024;
	if (curTable->pages[page] == -1) 
	{
		numFaults++; 
		ssd->numFaults++; 
	}
	numReqs++; 
	ssd->numReqs++; 
}

float myAgeInMS() 
{
	ull nsDiff;
	ui sDiff = ssd->timer.sec - thisAge.sec;
	if (ssd->timer.nsec >= thisAge.nsec) 
	{
		nsDiff = ssd->timer.nsec - thisAge.nsec;
	}
	else {
		nsDiff = sectons(1) - (thisAge.nsec - ssd->timer.nsec);
		sDiff--;
	}
	float ms = stoms(sDiff) + nstoms(nsDiff);
	return ms;

}
void printResults() {
	ull avgAccessNS = (((numReqs-numFaults)*10) + (numFaults * mstons(15))) / numReqs;
	double avgAccessMS = nstoms(avgAccessNS);
	writeLog("=======================================\n");
	writeLog("Process=%d\tIndex=%d Report\n", getpid(),curIndex);
	writeLog("TAT   : %.2fms\n", myAgeInMS());	
	writeLog("REQUESTS    : %d\n", numReqs);
	writeLog("PAGE FAULTS : %d\n", numFaults);
	if (avgAccessMS > 1.0)
		writeLog("AVG ACCESS  : %.2fms\n", avgAccessMS);
	else
		writeLog("AVG ACCESS  : %lluns\n", avgAccessNS);
	writeLog("PAGES USED  : %d\n", curTable->numPages);
	writeLog("=======================================\n\n");

}
void cleanup() 
{
	printResults();
	ssd->procStatFlag[curIndex] = 2;	
	exit(0);
}



void sigHandler(int signo) 
{
	if (signo == SIGINT) 
	{
		return;
	}
	else if (signo == SIGALRM) 
	{
		writeLog("User Process %d - Killed.\n", curIndex);
		cleanup();
	}
	else if (signo == SIGUSR1) 
	{
		writeLog("User Process %d - Killed.\n", curIndex);
		cleanup();
	}
}
