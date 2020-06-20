CC=c++
LINKER=
FLAGS=-Wall -Wextra -O3
OBJ=obj/dump1090.c
SRC=dump1090.c
dump: $(OBJ)
	$(CC) $(FLAGS) -o bin/dump $(OBJ) $(LINKER)

obj/dump1090.c: dump1090.c
	$(CC) $(FLAGS) -c dump1090.c -o obj/dump1090.c $(LINKER)

clean:
	rm obj/dump1090.c

