#!/bin/bash

openssl s_server -accept 4433 -cert server.crt -key server.key
