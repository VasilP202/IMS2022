CPP=g++
CFLAGS=-std=c++98 -Wall -pedantic -Wextra -lsimlib -lm
LFLAGS=-c


all: model

model.o:
	$(CPP) $(LFLAGS) model.cpp

model: model.o
	$(CPP) $(CFLAGS) model.o -o model

clean:
	rm *.o model