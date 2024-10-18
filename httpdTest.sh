#!/bin/bash

SSID=$1
PASSWD=$2

curl -k -H "Content-Type: app" -d "{ssid: $SSID, passwd: $PASSWD}" -X POST https://192.168.1.149/config
