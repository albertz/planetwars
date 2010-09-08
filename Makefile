CC=g++

all: MyBot.bin OrigMyBot.bin

clean:
	rm -rf *.o *.bin

MyBot.bin: MyBot.o PlanetWars.o
OrigMyBot.bin: OrigMyBot.o PlanetWars.o
