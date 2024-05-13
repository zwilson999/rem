build:
	gcc -Wall -o .\bin\rem.exe .\src\timer.c main.c -lncurses -DNCURSES_STATIC -I.\include
run:
	.\bin\rem.exe
