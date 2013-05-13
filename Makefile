#
#	File:		Makefile
#	Description:	For making the project
#	Authors:	Scott Connell && Joel Rausch
#	Date Created:	March 30, 2013 at 15:02
#
#	This file written for Programming Assignment 2 for CprE 426.
#	Iowa State University
#

CC= mpicxx
BIN=qsort
NP=16
INPUTFILE=input.txt
CFILES=	main.cpp	\
	sorter.cpp	\
	util.cpp	\

all:	$(CFILES)
	$(CC) -O3 -o $(BIN) $(CFILES)

run:
	mpirun -np $(NP) $(BIN) $(INPUTFILE)

clean:
	rm -f *.o
	rm -f qsort
	rm -f gen


#
# This to create a generator for an input file
# use as "./gen 10 > input.txt"
#
gen:	gen.cpp
	g++ -o gen gen.cpp
