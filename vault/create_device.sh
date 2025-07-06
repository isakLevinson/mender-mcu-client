export VAULT_ADDR=http://127.0.0.1:8200

vault write auth/approle/role/my-app-role \
  token_policies="csr_signer" \
  token_ttl=24h \
  token_max_ttl=100h \
  secret_id_ttl=100h \
  secret_id_num_uses=1000

vault policy write csr_signer ./csr_signer.hcl

