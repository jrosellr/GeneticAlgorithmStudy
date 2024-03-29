#!/bin/bash -l
#SBATCH --exclusive
#SBATCH -p aolin.q

hostname
echo

# Configure environment
module add gcc/9.2.0

# Read parameters
filename=$1
epochs=$2
seed=$3

# Compile
gcc -Ofast -fopenmp TSP.c -o $filename

perf stat -d $filename $epochs $seed 2>&1
perf record -o $filename.data $filename $epochs $seed 2>&1
