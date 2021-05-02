GCC=/usr/local/bin/gcc

simplefs: shell.o fs.o disk.o
	$(GCC) shell.o fs.o disk.o -o simplefs -lm -g

shell.o: shell.c
	$(GCC) -Wall shell.c -c -o shell.o -g

fs.o: fs.c fs.h
	$(GCC) -Wall fs.c -c -o fs.o -g -std=c99

disk.o: disk.c disk.h
	$(GCC) -Wall disk.c -c -o disk.o -g

clean:
	rm simplefs disk.o fs.o shell.o
