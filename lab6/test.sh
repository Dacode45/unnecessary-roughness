#!/bin/bash
set -e

echo 'Expects a traces/ folder.'

make CFLAGS='-std=c99 -g -DDEBUG -Wall -m32'
# make CFLAGS='-O2 -DNDEBUG -Wall -m32'

./mdriver -vl -f short1-bal.rep
# ./mdriver -vl -f short2-bal.rep

# ./mdriver -vl -t traces/
