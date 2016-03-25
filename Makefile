CC=g++-5
STD=c++14
CCFLAGS=-Wall -Werror
LDFLAGS=
OPT=
CMD=$(CC) --std=$(STD) $(OPT) -Iinclude

INC=$(wildcard include/*.h)
SRC=$(wildcard src/*.cpp)
OBJ=$(SRC:src/%.cpp=obj/%.o)
EXE=bin/incdb

all: bin/incdb

$(EXE): $(OBJ)
	$(CMD) $(LDFLAGS) $(OBJ) -o $(EXE)

obj/%.o: src/%.cpp
	$(CMD) $(CCFLAGS) -c $< -o $@

obj/incdb.o: include/allocator.h include/bufmgr.h
obj/allocator.o: include/allocator.h
obj/bufmgr.o: include/allocator.h include/bufmgr.h include/frame.h include/replacer.h
