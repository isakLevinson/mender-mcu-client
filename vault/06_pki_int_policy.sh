#!/bin/sh
set -o xtrace

#create a new policy to create update revoke and list certificates
vault policy write pki_int pki_int.hcl
