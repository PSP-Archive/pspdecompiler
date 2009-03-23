#!/usr/bin/make

CC=gcc
CFLAGS=	-O2 -g -Wall -ansi -pedantic
LIBS = -lexpat

OBJS = allegrex.o analyser.o output.o prx.o nids.o hash.o llist.o utils.o main.o
TARGET = pspdecompiler

all:	$(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LIBS)

.c.o:
	$(CC) -c -o $@ $(CFLAGS) $< 

.PHONY: clean

clean:
	rm -f $(OBJS) $(TARGET)
