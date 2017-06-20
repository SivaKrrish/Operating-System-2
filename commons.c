#include "commons.h"

extern SystemShare *ssd;
char *LOGFILENAME = "oss.log";

int setSharedMemory() 
{
	int id;
	key_t key = ftok("./oss.c", 60);
	int memNeeded = sizeof(SystemShare);
	if ((id = shmget(key, memNeeded, IPC_CREAT | 0666)) == -1) 
	{
		perror("Failed to create shared memory segment");
		exit(1);
	}
	if ((ssd = (SystemShare*)shmat(id, NULL, 0)) == (void *)-1) 
	{
		perror("Failed to attach to shared memory space");
		exit(1);
	}
	return id;
}

semFunction(int semid, int index, int val) 
{
	struct sembuf sembuff;
	sembuff.sem_num = index;
	sembuff.sem_op = val;
	sembuff.sem_flg = SEM_UNDO;
	semop(semid, &sembuff, 1);
}
void semWait(int semid, int index) 
{
	semFunction(semid, index, -1);
}

void semSig(int semid, int index) 
{
	semFunction(semid, index, 1);
}

//set the signal handler
void setSigHandler( void handler() ) 
{
	struct sigaction act;
	act.sa_handler = handler;
	act.sa_flags = 0;
	sigaction(SIGQUIT, &act, NULL);
	sigaction(SIGALRM, &act, NULL);
	sigaction(SIGINT, &act, NULL);
	sigaction(SIGSTOP, &act, NULL);
	sigaction(SIGABRT, &act, NULL);
	sigaction(SIGHUP, &act, NULL);
	sigaction(SIGUSR1, &act, NULL);
}

void initLog() 
{	
	if (DEBUG == 1) 
	{
		char *cmd = malloc(30);
		int x = sprintf(cmd, "rm %s 2>/dev/null", LOGFILENAME);
		system(cmd);
		free(cmd);
	}
}

char *curTime() 
{
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);
	char *currentTime = malloc(9);
	int x = sprintf(currentTime, "%d:%d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
	return currentTime;
}


void debugLog(const char * format, ...) 
{
	char buffer[500];
	va_list args;
	va_start(args, format);
	vsprintf(buffer, format, args);
	fprintf(stderr, "%ds%2.0fms %s", ssd->timer.sec, nstoms(ssd->timer.nsec), buffer);
	va_end(args);
}

void writeLog (const char * format, ...) 
{	
		char buffer[500];
		va_list args;
		va_start(args, format);
		vsprintf(buffer, format, args);
		if (DEBUG == 1) 
		{
			FILE *logFile = fopen(LOGFILENAME, "a");
			fprintf(logFile, "%s", buffer);
			fclose(logFile);
		}
		else
			fprintf(stderr, "%s", buffer);

		va_end(args);	
		
}




//function to take nanoseconds and return its value in seconds
float nstosec(ull ns) 
{
	float sec = (float)ns / 1000000000.0;
	return sec;
}

//function to take nanoseconds and return its value in milliseconds
float nstoms(ull ns) 
{
	float ms = (float)ns / 1000000.0;
	return ms;
}

ull sectons(int sec) 
{
	ull ns = sec * 1000000000;
	return ns;
}

ull stoms(int s) 
{
	ull ms = s * 1000;
	return ms;
}

float mstos(float ms) 
{
	float sec = (float)ms/1000.0;
	return sec;
}

ull mstons(float ms) 
{
	ull ns = ms * 1000000;
	return ns;
}
int isInt(char *str) 
{
	char *pEnd;
	errno=0;
	long int x = strtol(str, &pEnd, 10);
	if (pEnd == str) 
	{ 
		return 0;
	}
	return 1; 
}
int getDigitCount(int n) 
{
	int digitcnt=0;
	while (n!=0) 
	{
		n/=10;
		digitcnt++;
	}
	return digitcnt;
}
char* Itoa(int value, char* str, int radix) 
{
	static char dig[] ="0123456789abcdefghijklmnopqrstuvwxyz";
	int n = 0, neg = 0;
	unsigned int v;
	char* p, *q;
	char c;
	if (radix == 10 && value < 0) 
	{
		value = -value;
		neg = 1;
	}
	v = value;
	do 
	{
		str[n++] = dig[v%radix];
		v /= radix;
	}while (v);
	if (neg)
		str[n++] = '-';
	str[n] = '\0';
	for (p = str, q = p + (n-1); p < q; ++p, --q)
		c = *p, *p = *q, *q = c;
	return str;
}


