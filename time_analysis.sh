#!/bin/bash

# script for recording kernel execution time
# first arg is the fib function number, which 0 is dp and 1 is fast doubling.
# second arg is the offset
cd fd_data

for ((i=0; i<=1000; i+=2))
do
	filename="${i}.txt"
	touch "$filename"

	sudo taskset -c 0,1 ../client 1 "$i" >> "$filename"
done

cd ../dp_data

for((i=0; i<=1000; i+=2))
do
	filename="${i}.txt"
	touch "$filename"
	
	sudo taskset -c 0,1 ../client 0 "$i" >> "$filename"
done
