#!/bin/bash

IFS=','

count=0

echo '7000000000'

while true
do
	read -a arr
	if [ $? -ne 0 ]
	then
		echo 'EOF'
		exit 0
	fi



#	echo ${arr[0]}
	printf "70%08x\n" ${arr[0]}
#	echo A1 ${arr[1]}
#	echo d = $((${arr[0]} - $count))
	count=${arr[0]}


done

