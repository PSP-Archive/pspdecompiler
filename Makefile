#!/usr/bin/make

CC=gcc
CFLAGS=	-O2 -g -Wall -ansi -pedantic
LIBS = 

OBJS = allegrex.o analyser.o prx.o hash.o utils.o main.o
TARGET = pspdecompiler

all:	$(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LIBS)

.c.o:
	$(CC) -c -o $@ $(CFLAGS) $< 

.PHONY: clean

clean:
	rm -f $(OBJS) $(TARGET)
