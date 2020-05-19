#!/bin/sh
make SETUP=$1 clean
make SETUP=$1 test
./test.x
