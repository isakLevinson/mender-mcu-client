#!/bin/bash

openssl s_client -connect $1:1000 -cert client.crt -key client.key

