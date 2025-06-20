#!/bin/bash

HOST=$1
#DATA=$2
DATA=$(cat $2)

echo "HOST: $HOST"
echo "sending: $DATA"

curl -k -H "Content-Type: app" -d "$DATA" -X POST "https://$HOST/config"

