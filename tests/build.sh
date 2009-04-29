#!/bin/sh

cd ..
make clean
make
cd tests

rm graphs/*
../pspdecompiler $* test.prx
../makedots.sh

