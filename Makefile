CC=g++

all:
	for i in *.cc; do make $${i/.cc/}; done

clean:
	rm -rf *.o *Bot

%.o: %.cc
	g++ -Wall -O2 $< -c -o $@

%::
	if [ $@ != PlanetWars ]; then \
	make PlanetWars.o && \
	make $@.o && \
	g++ $@.o PlanetWars.o -o $@ ;\
	fi

