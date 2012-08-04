// PI calculus trough Monte Carlo method
//João Ferreira 2009113274
//Pedro Cristina Marques 2007184032

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <stdbool.h>

#include "worker.h"

#define processes 2
#define resolution 100000000

int fds[processes][2];
int pids[processes];
bool debugMode = true;

double accept_requests()
{
	int inside=0, temp;
	
	fd_set reader;
	
	int i, j;
	
	if (debugMode)
		printf("Daddy started to select messages!\n");
	
	for (j=0 ; j<processes ;)
	{
		FD_ZERO(&reader);
	
		for (i=0 ; i<processes ; i++)
			FD_SET(fds[i][0], &reader);
	
		if ( select(fds[processes-1][0]+1, &reader, NULL, NULL, NULL) != -1 )
		{
			for (i=0 ; i<processes ; i++)
			{
				if ( FD_ISSET(fds[i][0], &reader) )
				{
					if (read(fds[i][0], &temp, sizeof(int)) > 0 )
					{
						inside+=temp;
						
						if (debugMode)
							printf("DAD Says: I got %d points from process with id->%d\n", temp, i+1);
						
						j++;
					}
				}
			}
		}
	}
	
	return (double)inside;
}

void master_sigusr1_handler(int signum)
{
	signal(SIGUSR1, master_sigusr1_handler);
	if (debugMode)
		printf("Signal SIGUSR1 got caught!\n");
}

void master_sigint_handler(int signum)
{
	signal(SIGINT, master_sigint_handler);
	
	if (debugMode)
		printf("=> Signal SIGINT got caught!\nI Shall Die!\n");
	
	int i;
	
	for (i=0 ; i<processes ; i++)
	{
		kill(pids[i], SIGUSR1);
		close(fds[i][0]);
		wait(NULL);
	}
	
	exit(0);
}

int main (int argc, const char * argv[])
{
	double pi, inside;
	int i;
	
	signal(SIGUSR1, master_sigusr1_handler);
	signal(SIGINT, master_sigint_handler);
	sigset_t block_allsig;
	sigfillset(&block_allsig);
	sigdelset(&block_allsig, SIGUSR1);
	sigdelset(&block_allsig, SIGINT);
	sigprocmask(SIG_BLOCK, &block_allsig, NULL);
	
	for (i=0 ; i<processes ; i++)
		pipe(fds[i]);
	
	for (i=0 ; i<processes ; i++)
	{
		pids[i] = fork();
		
		if (pids[i] == 0)
		{
			if (debugMode)
				printf("[%d] I'm the son, my parent is [%d]\n", getpid(), getppid());
			
			worker(i, resolution/processes, processes, fds, debugMode);
			
			printf("Le wild bug appear, u should be in evaluation!\n");
		}
		
		else
			close(fds[i][1]);
	}
	
	inside = accept_requests(processes, fds);
	
	for (i=0 ; i<processes ; i++)
	{
		if (debugMode)
			printf("DAD: Hum... time to kill [%d]\n", pids[i]);
		
		kill(pids[i], SIGUSR1);
		close(fds[i][0]);
		wait(NULL);
	}
	
	if (debugMode)
	{
		printf("To ensure that all processes have died:\n");
		printf("Doing 'ps aux | grep monteCarlo' only three should appear, thats grep, the system() call, and master\n");
		system("ps aux | grep monteCarlo");
	}
	
	pi = 4 * inside/resolution;
	
	printf("PI = %f\n", pi);
	
	sigprocmask(SIG_UNBLOCK, &block_allsig, NULL);
	
	return 0;
}

