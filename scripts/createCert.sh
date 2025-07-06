#!/bin/bash

CA_PEM=""

# create CA cert
# Will be created by Vault
#openssl genpkey -algorithm RSA -out ca.key -pkeyopt rsa_keygen_bits:2048
#openssl req -new -key ca.key -out ca.csr -subj "/CN=CA"
#self sign
#openssl x509 -req -in ca.csr -signkey ca.key -out ca.crt -days 3650
#rm ca.csr

# create server cert
# Device will receive certificate from vault
#openssl genpkey -algorithm RSA -out server.key -pkeyopt rsa_keygen_bits:2048
#openssl req -new -key server.key -out server.csr -subj "/CN=pnu"
# sign
#openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out server.crt -days 365
#rm server.csr

openssl genpkey -algorithm RSA -out client.key -pkeyopt rsa_keygen_bits:2048
openssl req -new -key client.key -out client.csr -subj "/CN=client"
# sign
openssl x509 -req -in client.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out client.crt -days 365
rm client.csr

