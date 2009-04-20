#!/bin/sh

cd ..
make clean
make
cd tests

rm graphs/*
../pspdecompiler -v -g test.prx
../makedots.sh

