CC=g++

all: MyBot OrigMyBot

clean:
	rm -rf *.o *.bin

MyBot: MyBot.o PlanetWars.o
OrigMyBot: OrigMyBot.o PlanetWars.o
