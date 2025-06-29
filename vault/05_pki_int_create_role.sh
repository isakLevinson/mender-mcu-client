#!/bin/sh
set -o xtrace

#create a role to generate new certificates
vault write pki_int/roles/brain-space \
	allow_any_name=true \
	allow_bare_domains=true \
	allow_renewal=true \
	generate_lease=true \
    enforce_hostnames=false \
    use_csr_common_name=true \
    require_cn=false \
    max_ttl="8760h" \
    ttl="720h"

#vault delete pki_int/roles/brain-space
