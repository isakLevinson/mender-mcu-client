#!/bin/bash 

./r.sh < f1 | stdbuf --output=0 xxd -r -p | nc -u 192.168.1.9 5000 > f1
