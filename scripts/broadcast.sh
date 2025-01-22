#!/bin/bash

DATE=$(date +%s%6N)

echo sending $DATE

echo $DATE | nc -ub 192.168.1.255 5000

