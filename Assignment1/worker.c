//João Ferreira 2009113274
//Pedro Cristina Marques 2007184032

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>

int myId;
int myWrite;
bool debugMode;

int isInsideCircle(float x,float y)
{
	return (sqrt( pow(x,2) + pow(y,2)) <= 1) ? 1 : 0;
}

void setupRand()
{
	time_t seconds;
	time(&seconds);
	srand(((unsigned int) seconds) + getpid());
}

float randint()
{
	return rand() / (float) RAND_MAX;
}

void son_sigusr1_handler(int signum)
{
	if (debugMode)
		printf("[%d]Oh no! Daddy sent me a SIGUSR1, that means thats time to go :( bye bye\n", getpid());
	
	close(myWrite);
	exit(0);
}

void worker(int id, int points, int processes, int fds[][2], bool dM)
{
	int i, inside=0;
	
	myId = id;
	myWrite = fds[id][1];
	debugMode = dM;
	
	signal(SIGUSR1, son_sigusr1_handler);
	sigset_t block_allsig;
	sigfillset(&block_allsig);
	sigdelset(&block_allsig, SIGUSR1);
	sigprocmask(SIG_BLOCK, &block_allsig, NULL);
	
	for(i=0 ; i<processes ; i++)
	{
		if(i != id)
		{
			close(fds[i][0]);
			close(fds[i][1]);
		}
		
		else
			close(fds[i][0]);
	}
	
	setupRand();
	
	for (i=0 ; i<points ; i++)
	{
		float x = randint();
		float y = randint();
		inside += isInsideCircle(x, y);
	}
	
	if (debugMode)
		printf("[%d] I worked! My id is %d and i got %d points inside circle!\n", getpid(), id+1, inside);
	
	write(fds[id][1], &inside, sizeof(int));
	
	pause();
}
