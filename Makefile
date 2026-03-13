SFML_PATH=/usr/local/Cellar/sfml/2.6.1/
CXXFLAGS= -std=c++14 -Wall -Wpedantic -I${SFML_PATH}include/
LDFLAGS=-L${SFML_PATH}lib/
CFLAGS=-g -lsfml-graphics -lsfml-window -lsfml-system -lsfml-network -pthread
CPPFLAGS=
LDLIBS=
LIBS=
CPP=g++

all: server

server: GameServer.o
	$(CPP) $(CXXFLAGS) $(LDFLAGS) $(LIBS) $^ -o $@ $(CFLAGS)

clean:
	\rm -f *.o server
