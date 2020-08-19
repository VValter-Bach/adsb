CC=gcc
LINKER=$(shell pkg-config --libs librtlsdr) -lpthread -lm
FLAGS=-Wall -Wextra -O3 $(shell pkg-config --cflags librtlsdr)
OBJ=obj/decode.o obj/sdr.o obj/interactive.o obj/main.o obj/gps.o
SRC=decode.c sdr.c interactive.c main.c gps.c
adsb: $(OBJ)
	$(CC) $(FLAGS) -o bin/adsb $(OBJ) $(LINKER)

obj/decode.o: decode.c
	$(CC) $(FLAGS) -c decode.c -o obj/decode.o $(LINKER)

obj/sdr.o: sdr.c
	$(CC) $(FLAGS) -c sdr.c -o obj/sdr.o $(LINKER)

obj/interactive.o: interactive.c
	$(CC) $(FLAGS) -c interactive.c -o obj/interactive.o $(LINKER)

obj/main.o: main.c
	$(CC) $(FLAGS) -c main.c -o obj/main.o $(LINKER)

obj/gps.o: gps.c
	$(CC) $(FLAGS) -c gps.c -o obj/gps.o $(LINKER)

clean:
	rm obj/decode.o obj/sdr.o obj/interactive.o obj/main.o obj/gps.o

