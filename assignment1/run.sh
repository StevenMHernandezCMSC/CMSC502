#!/usr/bin/env bash

make clean

make sequential
make threaded
make mpi

## Time tests (sequential)
#echo "\c" > tmp/seq_runs.txt
#for ((i=10;i<=25;i++)); do
#    sh gen_cities.sh $i > tmp.txt
#    echo "run $i: \c" >> tmp/seq_runs.txt
#    ./sequential tmp.txt >> tmp/seq_runs.txt
#done

## Time tests (thread)
#echo "\c" > tmp/thread_runs.txt
#for ((i=25;i<=250;i++)); do
#    sh gen_cities.sh $i > tmp.txt
#    echo "run $i: \c" >> tmp/thread_runs.txt
#    ./threaded tmp.txt >> tmp/thread_runs.txt
#done

## Time tests (mpi)
#echo "\c" > tmp/mpi_runs.txt
#for ((i=10;i<=250;i++)); do
#    sh gen_cities.sh $i > tmp.txt
#    echo "run $i: \c" >> tmp/mpi_runs.txt
#    mpirun -np 4 ./mpi tmp.txt >> tmp/mpi_runs.txt
#done

echo "done with timing runs"


echo "starting accuracy runs"

# Time tests (sequential)
echo "\c" > tmp/accuracy_runs.txt
for ((i=15;i<=25;i++)); do
    sh gen_cities.sh $i > tmp.txt

    echo "run $i: \c" >> tmp/accuracy_runs.txt

    ./sequential tmp.txt >> tmp/accuracy_runs.txt
    echo ", \c" >> tmp/accuracy_runs.txt
    ./threaded tmp.txt >> tmp/accuracy_runs.txt
    echo ", \c" >> tmp/accuracy_runs.txt
    mpirun -np 4 ./mpi tmp.txt >> tmp/accuracy_runs.txt
    echo "" >> tmp/accuracy_runs.txt
done

echo "done with accuracy runs"
