#!/bin/bash

make

# read = 100
# write = 0
# push = 0
# pop = 0
# seed = 42

function test() {
    # read = $1  
    # write = $2
    # push = $3
    # pop = $4
    # seed = $5
    echo "START_TEST"

    echo "locked tests | seed: $5 | pools: 1 | ${1}+ / ${2}- / ${3}w / ${4}r"
    echo "START_PART"

    echo "STL-MTX"
    for threads in 1 2 4 8 16 32; do
        ./vec_sim.out -s -l -threads "$threads" -pools 1 -seed "$5" -push "$1" -pop "$2" -write "$3" -read "$4"
    done
    echo "END_PART"
    
    echo "lock_free tests | seed: $5 | pools: 1 | ${1}+ / ${2}- / ${3}w / ${4}r"
    echo "START_PART"

    echo "LF-P-1"
    for threads in 1 2 4 8 16 32; do
        ./vec_sim.out -s -lf -threads "$threads" -pools 1 -seed "$5" -push "$1" -pop "$2" -write "$3" -read "$4"
    done
    echo "END_PART"

    echo "lock_free tests | seed: $5 | pools: T | ${1}+ / ${2}- / ${3}w / ${4}r"

    echo "START_PART"
    echo "LF-P-T"
    for threads in 1 2 4 8 16 32; do
        ./vec_sim.out -s -lf -threads "$threads" -pools "$threads" -seed "$5" -push "$1" -pop "$2" -write "$3" -read "$4"
    done
    echo "END_PART"

    echo "lock_free tests | seed: $5 | pools: LEAKS | ${1}+ / ${2}- / ${3}w / ${4}r"
    echo "START_PART"

    echo "LF-LEAKS"
    for threads in 1 2 4 8 16 32; do
        ./vec_sim.out -s -lf -leak -threads "$threads" -pools "$threads" -seed "$5" -push "$1" -pop "$2" -write "$3" -read "$4"
    done
    echo "END_PART"

    echo "END_TEST"
}

#(pop,push,write,read)
test 15 5 10 70 42
test 15 0 15 70 42
test 30 20 20 30 42
test 35 0 35 30 42
test 15 5 10 70 42
test 15 0 15 70 42
test 0 0 0 100 42
test 50 0 5 45

