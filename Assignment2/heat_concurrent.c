/*
 *		main.c
 *		João Ferreira
 *		Pedro Cristina
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mman.h>

#define	FILE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
#define TOTAL_STEPS 100
#define SQUARE_DIMENSION 2000
#define PROCESSES 2
#define SIZE SQUARE_DIMENSION*SQUARE_DIMENSION*15
#define DEBUG


typedef struct {
	int next, this;
	float matrix[2][SQUARE_DIMENSION][SQUARE_DIMENSION];
} mem_struct;

mem_struct *mem;
int shmid, pids[PROCESSES];
sem_t *mutex, *remainingLines, *change;

void init()
{
	#ifdef DEBUG
		printf("Creating Shared Memory!\n");
	#endif
	
	if ((shmid = shmget(IPC_PRIVATE, sizeof(mem_struct), IPC_CREAT|0700)) != -1)
	{
		mem = (mem_struct*) shmat(shmid, NULL, 0);
		mem->this = 0;
	} else {
		printf("Error creating Shared Memory!\n");
		exit(1);
	}
	
	#ifdef DEBUG
		printf("Creating Semaphores!\n");
	#endif
	
	sem_unlink("MUTEX");
	mutex = sem_open("MUTEX", O_CREAT|O_EXCL, 0700, 0);
	
	sem_unlink("REMAININGLINES");
	remainingLines = sem_open("REMAININGLINES", O_CREAT|O_EXCL, 0700, 0); 
	
	sem_unlink("CHANGE");
	change = sem_open("CHANGE", O_CREAT|O_EXCL, 0700, 0);
}

void terminate()
{
	#ifdef DEBUG
		printf("Closing Semaphores!\n");
	#endif
	
	sem_close(mutex);
	sem_unlink("MUTEX");
	sem_close(remainingLines);
	sem_unlink("REMAINING_LINES");
	sem_close(change);
	sem_unlink("CHANGE");
	
	#ifdef DEBUG
		printf("Removing Shared Memory!\n");
	#endif
	
	shmctl(shmid, IPC_RMID, NULL);
}


void write_to_file(int n) 
{ 
	int i, j, pos = 0, fdout, k, s;
	char number[64], file[32], *mmf;
	
	if (TOTAL_STEPS*0.25 == n)
		sprintf(file,"firstQuarter.csv");
	else if (TOTAL_STEPS*0.50 == n)
		sprintf(file,"halftime.csv");
	else if (TOTAL_STEPS*0.75 == n)
		sprintf(file,"thirdQuarter.csv");
	else
		sprintf(file,"final.csv");
	
	fdout = open(file, O_RDWR | O_CREAT | O_TRUNC, FILE_MODE);
	
	lseek(fdout, SIZE, SEEK_SET);
	
	write(fdout, "", 1);
	
	mmf = mmap(NULL, SIZE, PROT_WRITE, MAP_SHARED, fdout, 0);
	
	for (i=0 ; i<SQUARE_DIMENSION ; i++)
	{
		for (j=0 ; j<SQUARE_DIMENSION-1 ; j++)
		{
			sprintf(number, "%.2f," , mem->matrix[mem->this][i][j]);
	
			s = strlen(number);
			for(k = 0; k < s; k++)
				mmf[pos++] = number[k];
		}
		
		sprintf(number, "%.2f\n" , mem->matrix[mem->this][i][j]);
		s = strlen(number);
		for(k = 0; k < s; k++)
			mmf[pos++] = number[k];
	}
	
	munmap(mmf,SIZE);
	close(fdout);
	
	#ifdef DEBUG
		printf("Successfully Created File!\n");
	#endif
}


void worker(int step, int id)
{
	int y, x;
	float cx, cy;
	
	cx = cy = 0.1;
	
	while(1)
	{
		sem_wait(remainingLines);
		
		sem_wait(mutex);
		x = mem->next;
		mem->next++;
		sem_post(mutex);
		
		for (y = 1; y < SQUARE_DIMENSION - 1; y++)
		{
			mem->matrix [!mem->this][x][y] = mem->matrix [mem->this][x][y] +
			cx * (mem->matrix [mem->this][x+1][y] + mem->matrix [mem->this][x-1][y] - 2 * mem->matrix [mem->this][x][y]) +
			cy * (mem->matrix [mem->this][x][y+1] + mem->matrix [mem->this][x][y-1] - 2 * mem->matrix [mem->this][x][y]);
		}
		
		sem_wait(mutex);
		if(x == SQUARE_DIMENSION-1)
		{
			sem_post(change);
		}
		sem_post(mutex);
	}
}

void loadMatrix()
{
	int i, j;
	FILE *f;
	
	if (!(f = fopen("temperature2000.csv", "r")))
	{
		printf("Error opening input file!\n");
		exit(1);
	}
	
	#ifdef DEBUG
		printf("Loading Matrix\n");
	#endif
	
	for (i = 0 ; i<SQUARE_DIMENSION ; i++)
	{
		for (j = 0 ; j<SQUARE_DIMENSION ; j++)
		{
			fscanf(f, "%f", &mem->matrix[mem->this][i][j]);
			fgetc(f);
		}
	}
	
	fclose(f);
	
	#ifdef DEBUG
		printf("Matrix Loaded\n");
	#endif
}

int main(int argc, char** argv)
{
	int i, n_steps, m;
	
	init();
	
	loadMatrix();
	
	n_steps = 1;
	
	for (n_steps =1; n_steps <= TOTAL_STEPS ; n_steps++)
	{
		#ifdef DEBUG
			printf("Step: %d\n", n_steps);
		#endif
		
		mem->next = 1;
		
		for (i=1 ; i<SQUARE_DIMENSION ; i++)
			sem_post(remainingLines);
		
		sem_getvalue(mutex, &m);
		if (m == 0)
			sem_post(mutex);
		
		for (i=0 ; i<PROCESSES ; i++)
		{
			pids[i] = fork();
			if (pids[i] == 0)
			{
				worker(n_steps, i);
				exit(0);
			}
		}
		
		sem_wait(change);
		
		for (i=0 ; i<PROCESSES ; i++)
		{
			kill(pids[i],SIGKILL);
			wait(NULL);
		}
		
		if (n_steps == TOTAL_STEPS*0.25 || n_steps == TOTAL_STEPS*0.5 || n_steps == TOTAL_STEPS*0.75 || n_steps == TOTAL_STEPS)
			write_to_file(n_steps);
		
		mem->this = !mem->this;
	}
	
	terminate();
	
	printf("All Done!\n");
	
	return 0;
}
