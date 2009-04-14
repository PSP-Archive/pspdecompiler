#!/usr/bin/make

CC=gcc
CFLAGS=	-O0 -g -Wall -ansi -pedantic
LIBS = -lexpat

OBJS = allegrex.o analyser.o decoder.o switches.o subs.o output.o code.o prx.o nids.o hash.o lists.o alloc.o utils.o main.o
TARGET = pspdecompiler

all:	$(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LIBS)

.c.o:
	$(CC) -c -o $@ $(CFLAGS) $< 


.PHONY: clean

clean:
	rm -f $(OBJS) $(TARGET)
