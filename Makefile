build:
	gcc -Wall -Wextra -g3  -I.\include -o .\bin\rem.exe .\src\timer.c main.c -lncurses -DNCURSES_STATIC
run:
	.\bin\rem.exe
