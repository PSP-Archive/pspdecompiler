#!/usr/bin/make

CC=gcc
CFLAGS=	-O2 -g -Wall -ansi -pedantic
LIBS = -lexpat

OBJS = allegrex.o analyser.o output.o prx.o nids.o hash.o lists.o alloc.o utils.o main.o
TARGET = pspdecompiler

.c.o:
	$(CC) -c -o $@ $(CFLAGS) $< 

all:	$(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LIBS)


.PHONY: clean

clean:
	rm -f $(OBJS) $(TARGET)
