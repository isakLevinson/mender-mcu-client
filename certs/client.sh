#!/bin/bash

openssl s_client -connect 127.0.0.1:4433 -cert client.crt -key client.key

