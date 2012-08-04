#João Ferreira 2009113274
#Pedro Cristina Marques 2007184032

FLAGS= -Wall -lm -g

all: monteCarlo

clean:
	rm -f *.o ~.*

monteCarlo: monteCarlo.o worker.o
	gcc monteCarlo.o worker.o ${FLAGS} -o monteCarlo

monteCarlo.o: monteCarlo.c
	gcc monteCarlo.c ${FLAGS} -c -o monteCarlo.o

worker.o: worker.c
	gcc worker.c ${FLAGS} -c -o worker.o
