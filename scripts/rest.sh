#!/bin/bash

HOST=$1
DATA=$2

curl -v -k -H "Content-Type: app" -d "$DATA" -X POST "https://$HOST/rest"

