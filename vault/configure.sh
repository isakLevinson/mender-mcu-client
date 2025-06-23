#!/bin/bash

./03_pki_root_ca_generate.sh
./04_pki_intermediate_ca_generate.sh
./05_pki_int_create_role.sh
./07_userpass_create.sh
./06_pki_int_policy.sh
./08_generate_certificate.sh isa


