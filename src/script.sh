#!/bin/sh
make clean
make
bunzip2 -kc ../traces/int_1.bz2 | ./predictor --gshare:32
bunzip2 -kc ../traces/int_1.bz2 | ./predictor --tournament:32:16:16