CC=gcc
LINKER=$(shell pkg-config --libs librtlsdr) -lpthread -lm
FLAGS=-Wall -Wextra -O3 -fpermissive -fno-rtti -fno-exceptions $(shell pkg-config --cflags librtlsdr)
OBJ=obj/dump1090.o
SRC=dump1090.c
dump: $(OBJ)
	$(CC) $(FLAGS) -o bin/dump $(OBJ) $(LINKER)

obj/dump1090.o: dump1090.c
	$(CC) $(FLAGS) -c dump1090.c -o obj/dump1090.o $(LINKER)

clean:
	rm obj/dump1090.o

