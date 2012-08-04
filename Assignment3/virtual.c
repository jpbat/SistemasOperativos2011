/*
 * João Ferreira 2009113274
 * Pedro Cristina 2007184032
 * Time Spent: 25h Aprox
 */

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/stat.h>

#define MAX_TEXT 20 //4060
#define MAX_PAGES 10

#define N_WORKERS 4
#define N_SENDERS 4
#define N_PRINTERS 3

#define SLEEP_TIME 3
//#define DEBUG

/*Structures*/
typedef struct segment {
	int id;
	int page;
	char text[MAX_TEXT];
	struct segment* next;
	bool last;
} segment;

typedef struct job {
	int id;
	int page;
	char txt[MAX_TEXT];
	bool end;
} job;

typedef struct queue {
	segment* head;
	segment* tail;
} queue;

typedef struct element {
	queue q;
	pthread_mutex_t mutex;
	pthread_cond_t work;
} element;

typedef struct received {
	queue q;
	bool ended;
	struct received* next;
} received;

typedef struct queueReceived {
	received* head;
	received* tail;
} queueReceived;


/*Global Stuff*/
element workers[N_WORKERS];
pthread_t threads[N_WORKERS], senders[N_SENDERS];
pthread_mutex_t display, pikachu;
char *printers[N_PRINTERS],
	 *abc = {"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"};
int fds[N_PRINTERS], workers_ids[N_WORKERS], senders_ids[N_SENDERS];
bool toShutdown = false;


/*Functions*/
segment* segment_init(int id, int page, char* text, bool end)
{
	segment* s = (segment*) malloc (sizeof(segment));
	
	s->next = NULL;
	s->page = page;
	s->id = id;
	s->last = end;
	strcpy(s->text, text);
	
	return s;
}

void queue_init(queue* q)
{
	q->head = NULL;
	q->tail = NULL;
}

void enqueue(queue* q, segment* s)
{
	if (q->tail)
		q->tail->next = s;
	
	q->tail = s;
	
	if (!q->head)
		q->head = s;
	
	s->next = NULL;
}

segment* dequeue(queue* q)
{
	segment* retval;
	
	retval = q->head;
	
	if (q->head)
		q->head = q->head->next;
	
	if (q->tail == retval)
		q->tail = NULL;
	
	if (retval)
		retval->next = NULL;
	
	return retval;
}

received* received_init()
{
	received* r = (received*) malloc (sizeof(received));
	
	r->next = NULL;
	queue_init(&r->q);
	r->ended = false;
	
	return r;
}

void queueReceived_init(queueReceived* qR)
{
	qR->head = NULL;
	qR->tail = NULL;
}

received* dequeueReceived(queueReceived* qR)
{
	received* retval;
	
	retval = qR->head;
	
	if (qR->head)
		qR->head = qR->head->next;
	
	if (qR->tail == retval)
		qR->tail = NULL;
	
	if (retval)
		retval->next = NULL;
	
	return retval;
}

void enqueueReceived(queueReceived* qR, received* r)
{
	if (qR->tail)
		qR->tail->next = r;
	
	qR->tail = r;
	
	if (!qR->head)
		qR->head = r;
	
	r->next = NULL;
}

received* getJob(int id, queueReceived *qR)
{
	queueReceived* q = qR;
	received* retval;
	
	while (q->head)
	{
		retval = q->head;
		if (retval->q.head != NULL && retval->q.head->id == id)
			return retval;
		q->head = q->head->next;
	}
	return NULL;
}

void* worker(void* x)
{
	int id = *((int *) x), i, current=-1;
	element* me = &workers[id-1];
	queueReceived terminated;
	received *term;
	
	#ifdef DEBUG
		printf("[DEBUG] THREAD%d is now working\n", id);
	#endif
	
	queueReceived_init(&terminated);
	
	while(1)
	{
		pthread_mutex_lock(&me->mutex);
		if (me->q.head == NULL)
		{
			pthread_cond_wait(&me->work, &me->mutex);
		}
		pthread_mutex_unlock(&me->mutex);
		
		pthread_mutex_lock(&pikachu);
		if (toShutdown)
		{
			pthread_mutex_unlock(&pikachu);
			#ifdef DEBUG
				pthread_mutex_lock(&display);
					printf("[DEBUG] worker thread%d killed\n", id);
				pthread_mutex_unlock(&display);
			#endif
			
			for (i=0 ; i<N_WORKERS ; i++)
				pthread_cond_signal(&workers[i].work);
			
			pthread_exit(0);
		}
		pthread_mutex_unlock(&pikachu);
		
		pthread_mutex_lock(&me->mutex);
		while (me->q.head)
		{
			term = getJob(me->q.head->id, &terminated);
			
			if (term == NULL)
			{
				term = received_init();
				enqueueReceived(&terminated, term);
			}
			
			segment* s = dequeue(&me->q);
			if (s->last)
				term->ended = true;
			enqueue(&term->q, s);
		}
		pthread_mutex_unlock(&me->mutex);
		
		for (term=terminated.head ; term ; term=term->next)
		{
			if(term->ended && term->q.head)
			{
				pthread_mutex_lock(&display);
				printf("\n\n[THREAD%d] JOB %d\n", id, term->q.head->id);
				while (term->q.head)
				{
					segment* s = dequeue(&term->q);
					if (s->page != current)
					{
						current = s->page;
						printf("PAGE %d\n", current);
					}
					printf("%s\n", s->text);
				}
				pthread_mutex_unlock(&display);
			}
		}
	}
	pthread_exit(0);
}

void gotSignal(int signum)
{
	int i;
	
	#ifdef DEBUG
		printf(" [DEBUG] signal being handled\n");
	#endif
	
	pthread_mutex_lock(&pikachu);
	toShutdown = true;
	pthread_mutex_unlock(&pikachu);
	
	for (i=0 ; i<N_WORKERS ; i++)
		pthread_cond_signal(&workers[i].work);
	
	#ifdef DEBUG
		printf("[DEBUG] toShutdown is now true!\n");
	#endif
}

void init()
{
	int i;
	char name[64];
	
	for (i=0 ; i<N_PRINTERS ; i++)
	{
		sprintf(name, "PRINTER%d", i+1);
		printers[i] = (char*) malloc (strlen(name)*sizeof(char));
		strcpy(printers[i], name);
		
		unlink(printers[i]);
		
		if ( (mkfifo(printers[i], O_CREAT|O_EXCL|0600)<0) && (errno!= EEXIST) )
		{
			printf("[ERROR]: Cannot create pipe: %s\n", printers[i]);
			exit(0);
		}
		
		if ( (fds[i] = open(printers[i], O_RDWR)) < 0)
		{
			printf("[ERROR]: Cannot open pipe %s\n", printers[i]);
			exit(0);
		}
		
		#ifdef DEBUG
			printf("[DEBUG] created printer %s\n", printers[i]);
		#endif
	}
	
	for (i=0 ; i<N_WORKERS ; i++)
	{
		#ifdef DEBUG
			printf("[DEBUG] initializing values from thread %d\n", i+1);
		#endif
		
		pthread_mutex_init(&workers[i].mutex, NULL);
		pthread_cond_init(&workers[i].work, NULL);
		queue_init(&workers[i].q);
	}
	
	signal(SIGINT, gotSignal);
	
	pthread_mutex_init(&display, NULL);
	pthread_mutex_init(&pikachu, NULL);
}

void terminate()
{
	int i;
	
	for (i=0 ; i<N_WORKERS ; i++)
	{
		pthread_join(threads[i], NULL);
		
		#ifdef DEBUG
			printf("[DEBUG] worker thread %d died\n", i+1);
		#endif
		pthread_mutex_destroy(&workers[i].mutex);
		pthread_cond_destroy(&workers[i].work);
	}
	
	for (i=0 ; i<N_SENDERS ; i++)
	{
		pthread_join(senders[i], NULL);
		
		#ifdef DEBUG
			printf("[DEBUG] sender thread %d died\n", i+1);
		#endif
	}
	
	for(i=0 ; i<N_PRINTERS ; i++)
	{
		#ifdef DEBUG
			printf("[DEBUG] closing printer: %s \n", printers[i]);
		#endif
		
		close(fds[i]);
		remove(printers[i]);
	}
	
	pthread_mutex_destroy(&display);
	pthread_mutex_destroy(&pikachu);
}

void accept_requests()
{
	int i, myThread;
	job j;
	segment* s;
	
	#ifdef DEBUG
		printf("[DEBUG] starting to select\n");
	#endif
	
	while (1)
	{
		pthread_mutex_lock(&pikachu);
		if (toShutdown)
		{
			pthread_mutex_unlock(&pikachu);
			#ifdef DEBUG
				printf("[DEBUG] main THREAD left select()\n");
			#endif
			break;
		}
		pthread_mutex_unlock(&pikachu);
		
		fd_set read_set;
		FD_ZERO(&read_set);
		
		for (i=0; i<N_PRINTERS; i++)
			FD_SET(fds[i], &read_set);
		
		if ( select(fds[N_PRINTERS-1]+1, &read_set, NULL, NULL, NULL) > 0 )
		{
			for (i=0 ; i<N_PRINTERS ; i++)
			{
				if (FD_ISSET(fds[i], &read_set))
				{
					if (read(fds[i], &j, sizeof(j)) > 0)
					{
						s = segment_init(j.id, j.page, j.txt, j.end);
						#ifdef DEBUG
							printf("[DEBUG] Received job%d page%d\n", j.id, j.page);
						#endif
						
						myThread = j.id % N_WORKERS;
						
						pthread_mutex_lock(&workers[myThread].mutex);
							enqueue(&workers[myThread].q, s);
							pthread_cond_signal(&workers[myThread].work);
						pthread_mutex_unlock(&workers[myThread].mutex);
					}
				}
			}
		}
	}
}

void randomText(char* txt, int x, int textSize)
{
	int i;
	
	srand(time(NULL)+x);
	
	for (i=0 ; i<textSize-1 ; i++)
		txt[i] = abc[rand()%52];
	txt[i] = '\0';
}

void* send(void *x)
{
	job j;
	int id = *((int *) x), i, pages, k, myPrinter, textSize;
	
	for(i=id-1 ;  ; i+=N_SENDERS)
	{
		pthread_mutex_lock(&pikachu);
		if (toShutdown)
		{
			pthread_mutex_unlock(&pikachu);
			#ifdef DEBUG
				pthread_mutex_lock(&display);
					printf("[DEBUG] sender thread%d killed\n", id);
				pthread_mutex_unlock(&display);
			#endif
			pthread_exit(0);
		}
		pthread_mutex_unlock(&pikachu);
		
		j.id = i+1;
		srand(time(NULL)+i);
		pages = rand() % MAX_PAGES + 1;
		myPrinter = i % N_PRINTERS;
		
		for (k=0; k<pages ; k++)
		{
			textSize = rand() % (MAX_TEXT-1)+2;
			j.page = k+1;
			randomText(j.txt, i+k, textSize);
			j.end = false;
			if (k == pages-1)
				j.end = true;
			
			write(fds[myPrinter], &j, sizeof(job));
		}
		
		pthread_mutex_lock(&display);
		#ifdef DEBUG
			printf("[DEBUG] THREAD%d sent job %d with %d pages into %s\n", id, j.id, pages, printers[myPrinter]);
		#endif
		pthread_mutex_unlock(&display);
		
		sleep(SLEEP_TIME);
	}
	
	pthread_exit(0);
}

int main(int argc, char** argv)
{
	int i;
	
	init();
	
	for (i=0 ; i<N_WORKERS ; i++)
	{
		#ifdef DEBUG
			printf("[DEBUG] creating worker thread %d\n", i+1);
		#endif
		workers_ids[i] = i+1;
		pthread_create(&threads[i], NULL, worker, &workers_ids[i]);
	}
	
	for (i=0 ; i<N_SENDERS ; i++)
	{
		#ifdef DEBUG
			printf("[DEBUG] creating sender thread %d\n", i+1);
		#endif
		senders_ids[i] = i+1;
		pthread_create(&senders[i], NULL, send, &senders_ids[i]);
	}
	
	accept_requests();
	
	terminate();
	
	return 0;
}
