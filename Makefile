treeclone.exe : treeclone.c treeclonedbg.c main.c
	gcc -g -Wall -o $@ $^

clean :
	rm treeclone.exe
