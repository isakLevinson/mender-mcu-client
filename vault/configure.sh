#!/bin/bash

export VAULT_ADDR=http://127.0.0.1:8200
export VAULT_NAMESPACE=

./03_pki_root_ca_generate.sh
./04_pki_intermediate_ca_generate.sh
./05_pki_int_create_role.sh
./06_pki_int_policy.sh
#./07_userpass_create.sh
./08_generate_certificate.sh isa
vault auth enable approle
vault auth enable cert

./create_device.sh
./fetch_device_role.sh
./updateFactory.sh

#../scripts/writeConfigPatition.sh


