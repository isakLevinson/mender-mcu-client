#!/bin/bash

# Check for required parameters
if [ "$#" -ne 1 ]; then
  echo "Usage: $0 <device_name>"
  exit 1
fi

set -o xtrace
#set roleid and secretid as env variables from the previous step
export VAULT_USER="admin"
export VAULT_PASSWORD="secret"

echo '#0'

#vault secrets enable -path=pki_int pki

echo '#1'

vault login -format=json -method=userpass \
    username=${VAULT_USER} \
    password=${VAULT_PASSWORD} | jq -r .auth.client_token > user.token

#store the token as env variable, now this token can be used to authenticate against Vault
export VAULT_TOKEN=`cat user.token`
echo '#2'
#vault secrets enable -path=pki pki
echo '#3'
vault secrets tune -max-lease-ttl=87600h pki

echo '#4'

#Use the new token to generate a new certificate and store it in a file
vault write -format=json pki_int/issue/brain-space \
    common_name=$1 > certs/$1.crt

#extract the certificate, issuing ca in the pem file and private key in the key file seperately
cat certs/$1.crt | jq -r .data.certificate > certs/$1.pem
cat certs/$1.crt | jq -r .data.issuing_ca >> certs/$1.pem
cat certs/$1.crt | jq -r .data.private_key > certs/$1.key


