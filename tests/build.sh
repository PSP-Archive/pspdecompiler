#!/bin/sh

cd ..
make clean
make
cd tests

rm graphs/*
../pspdecompiler -v -t -r -d -f -p -q -g test.prx
../makedots.sh
