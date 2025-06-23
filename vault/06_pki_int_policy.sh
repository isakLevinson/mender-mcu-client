#!/bin/sh
set -o xtrace
export VAULT_ADDR=http://127.0.0.1:8200
export VAULT_NAMESPACE=

#create a new policy to create update revoke and list certificates
vault policy write pki_int myfiles/pki_int.hcl
