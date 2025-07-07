#!/bin/bash

KEY="certs/client.key"
CSR="certs/client.csr"

openssl genpkey -algorithm RSA -out "$KEY" -pkeyopt rsa_keygen_bits:2048
openssl req -new -key "$KEY" -out "$CSR" -subj "/CN=client"

