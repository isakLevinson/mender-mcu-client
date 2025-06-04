#!/bin/bash

HOST=$1
DATA=$2
SESSION_FILE="session_$HOST.pem"

if [ -f $SESSION_FILE ]; then
	SES_CMD="-sess_in $SESSION_FILE"
else
	SES_CMD="-sess_out $SESSION_FILE"
fi



(
echo -ne "POST /control/a HTTP/1.1\r\nHost: $HOST\r\nConnection: keep-alive\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 7\r\n\r\n$DATA";
sleep 1
) | openssl s_client -connect $HOST:443 $SES_CMD

