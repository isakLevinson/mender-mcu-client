#!/bin/bash

set -o errexit
set -o nounset
set -o pipefail

VAULT_ADDR="http://127.0.0.1:8200"
ROLE_NAME="brain-space"
CSR_FILE="$1.csr"
TTL="720h"
OUT_CERT="$1.pem"
VAULT_TOKEN=$(cat token.id)


# --- VALIDATION ---
echo "📄 CSR file: $CSR_FILE"
if [ ! -f "$CSR_FILE" ]; then
  echo "❌ CSR file '$CSR_FILE' not found. Exiting."
  exit 1
fi

# --- ESCAPE CSR CONTENT ---
ENCODED_CSR=$(sed ':a;N;$!ba;s/\n/\\n/g' "$CSR_FILE" | sed 's/"/\\"/g')

# --- JSON PAYLOAD ---
JSON_PAYLOAD="{\"csr\":\"$ENCODED_CSR\",\"ttl\":\"$TTL\"}"

# --- HTTP REQUEST TO VAULT ---
RESPONSE=$(curl -sS \
  --header "X-Vault-Token: $VAULT_TOKEN" \
  --request PUT \
  --data "$JSON_PAYLOAD" \
  "$VAULT_ADDR/v1/pki_int/sign/$ROLE_NAME")

# --- PARSE RESULT ---
CERT=$(echo "$RESPONSE" | jq -r .data.certificate)

if [ "$CERT" = "null" ]; then
  echo "❌ Error from Vault:"
  echo "$RESPONSE" | jq
  exit 1
fi

# --- SAVE OUTPUT ---
echo "$CERT" > "$OUT_CERT"
echo "✅ Certificate saved to $OUT_CERT"
