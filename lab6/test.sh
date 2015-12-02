#!/bin/bash
set -e

echo 'Expects a traces/ folder.'

make CFLAGS='-g -Wall -m32'

./mdriver -V -f short1-bal.rep
./mdriver -t traces/