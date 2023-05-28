master.o : master.c
	gcc -w master.c -o master.o -lm

porto.o : porto.c
	gcc -w porto.c -o porto.o

nave.o : nave.c
	gcc -w nave.c -o nave.o -lm