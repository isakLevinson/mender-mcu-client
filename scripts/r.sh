#!/bin/bash

IFS=','

count=0

echo '7000000000'

while true
do
	read -a arr
	if [ $? -ne 0 ]
	then
		exit 0
	fi

	count=${arr[0]}
	c0=$(($count & 255))
	c1=$((($count >> 8) & 255))
	c2=$((($count >> 16) & 255))
	c3=$((($count >> 24) & 255))
	
	printf "70%02x%02x%02x%02x\n" $c0 $c1 $c2 $c3
#	sleep 0.1
done

