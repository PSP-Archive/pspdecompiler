#!/usr/bin/make

CC=gcc
CFLAGS=	-Wall -ansi -pedantic -g -O0
LIBS = -lexpat

OBJS = allegrex.o analyser.o decoder.o switches.o subs.o ctrlflow.o output.o prx.o nids.o hash.o lists.o alloc.o utils.o main.o
TARGET = pspdecompiler

all:	$(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $< 


.PHONY: clean

clean:
	rm -f $(OBJS) $(TARGET)
