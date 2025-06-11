#!/bin/sh
# Check for required parameters
if [ "$#" -ne 1 ]; then
  echo "Usage: $0 <device_name>"
  exit 1
fi

set -o xtrace
export VAULT_ADDR=http://127.0.0.1:8200
#set roleid and secretid as env variables from the previous step
export VAULT_USER="admin"
export VAULT_PASSWORD="secret"
vault secrets enable -path=pki_int pki

vault login -format=json -method=userpass \
    username=${VAULT_USER} \
    password=${VAULT_PASSWORD} | jq -r .auth.client_token > user.token

#store the token as env variable, now this token can be used to authenticate against Vault
export VAULT_TOKEN=`cat user.token`

vault secrets enable -path=pki pki
vault secrets tune -max-lease-ttl=87600h pki


#Use the new token to generate a new certificate and store it in a file
vault write -format=json pki_int/issue/brain-space \
    common_name=$1 > certs/$1.crt

#extract the certificate, issuing ca in the pem file and private key in the key file seperately
cat certs/$1.crt | jq -r .data.certificate > certs/$1.pem
cat certs/$1.crt | jq -r .data.issuing_ca >> certs/$1.pem
cat certs/$1.crt | jq -r .data.private_key > certs/$1.key


