#!/bin/bash

openssl s_client -connect $1:443 -cert client.crt -key client.key

