#!/bin/sh
make clean
make
echo "running int_1.bz2 trace..."
bunzip2 -kc ../traces/int_1.bz2 | ./predictor --gshare:13
bunzip2 -kc ../traces/int_1.bz2 | ./predictor --tournament:9:10:10
echo "running int_2.bz2 trace..."
bunzip2 -kc ../traces/int_2.bz2 | ./predictor --gshare:13
bunzip2 -kc ../traces/int_2.bz2 | ./predictor --tournament:9:10:10
echo "running fp_1.bz2 trace..."
bunzip2 -kc ../traces/fp_1.bz2 | ./predictor --gshare:13
bunzip2 -kc ../traces/fp_1.bz2 | ./predictor --tournament:9:10:10
echo "running fp_2.bz2 trace..."
bunzip2 -kc ../traces/fp_2.bz2 | ./predictor --gshare:13
bunzip2 -kc ../traces/fp_2.bz2 | ./predictor --tournament:9:10:10
echo "running mm_1.bz2 trace..."
bunzip2 -kc ../traces/mm_1.bz2 | ./predictor --gshare:13
bunzip2 -kc ../traces/mm_1.bz2 | ./predictor --tournament:9:10:10
echo "running mm_2.bz2 trace..."
bunzip2 -kc ../traces/mm_2.bz2 | ./predictor --gshare:13
bunzip2 -kc ../traces/mm_2.bz2 | ./predictor --tournament:9:10:10