#João Ferreira 2009113274
#Pedro Cristina Marques 2007184032

FLAGS= -Wall -lm -g -D_REENTRANT -pthread

all: virtual

clean:
	rm -f *.o ~.*

virtual: virtual.o
	gcc virtual.o ${FLAGS} -o virtual

virtual.o: virtual.c
	gcc virtual.c ${FLAGS} -c -o virtual.o

