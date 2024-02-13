all: myshell looper mypipe mypipeline

mypipe: mypipe.o
	gcc -m32 -g -Wall -o mypipe mypipe.o

mypipe.o: mypipe.c
	gcc -m32 -g -Wall -c -o mypipe.o mypipe.c -I.

myshell: myshell.o LineParser.o
	gcc -m32 -g -Wall -o myshell myshell.o LineParser.o

looper: looper.o
	gcc -m32 -g -Wall -o looper looper.o

myshell.o: myshell.c LineParser.h
	gcc -m32 -g -Wall -c -o myshell.o myshell.c -I.

LineParser.o: LineParser.c LineParser.h
	gcc -m32 -g -Wall -c -o LineParser.o LineParser.c -I.

looper.o: looper.c
	gcc -m32 -g -Wall -c -o looper.o looper.c -I.

mypipeline: mypipeline.o 
	gcc -m32 -g -Wall -o mypipeline mypipeline.o 

mypipeline.o: mypipeline.c
	gcc -m32 -g -Wall -c -o mypipeline.o mypipeline.c

.PHONY: clean

clean:
	rm -f *.o myshell looper mypipe mypipeline