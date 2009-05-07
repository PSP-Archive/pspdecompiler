#!/bin/sh

cd ..
make clean
make
cd tests

rm graphs/*
../pspdecompiler -n nids.xml -c -vvv testprx.prx
../pspdecompiler -n nids.xml -g $* testprx.prx
../makedots.sh

