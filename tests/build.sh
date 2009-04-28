#!/bin/sh

cd ..
make clean
make
cd tests

rm graphs/*
../pspdecompiler -t -d -f -p -q -g test.prx
../makedots.sh
