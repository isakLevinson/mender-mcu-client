#!/bin/bash

HOST=$1
DATA=$2

#curl -v -k -H "Content-Type: app" -d "$DATA" -X POST "https://$HOST/rest"

(
echo -ne "POST /rest HTTP/1.1\r\nHost: $HOST\r\nConnection: keep-alive\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 7\r\n\r\n$DATA";
sleep 1
) | openssl s_client -connect pnu_5.local:443

