#!/bin/bash
set -e

echo 'Expects a traces/ folder.'

make clean
#make CFLAGS='-g -DDEBUG -Wall -m32'
make CFLAGS='-O2 -DNDEBUG -m32'

./mdriver -vl -f short1-bal.rep
./mdriver -vl -f short2-bal.rep

./mdriver -vVl -t traces/
