#!/bin/bash

mkdir -p gcc-builds

for file in *.c; do
    if [ -f "$file" ]; then
        output="gcc-builds/${file%.c}"
        if [[ "$file" == "sieve_pthread.c" ]]; then
            gcc -std=c99 -O2 "$file" -lpthread -o "$output"
        else
            gcc -std=c99 -O2 "$file" -o "$output"
        fi
    fi
done