#!/bin/bash

export VAULT_ADDR=http://127.0.0.1:8200

ROLE_ID=$(cat ./role_id)
SECRET_ID=$(cat ./secret_id)
APP_TOKEN_FILE="./app_token"

# Make sure ROLE_ID and SECRET_ID are already exported or defined above this

# Get the token and write it to file
vault write -format=json auth/approle/login role_id="$ROLE_ID" secret_id="$SECRET_ID" \
  | jq -r .auth.client_token | tee "$APP_TOKEN_FILE"

# Now export the token from the file
export APP_TOKEN=$(cat "$APP_TOKEN_FILE")

export VAULT_TOKEN=$APP_TOKEN
echo TOKEN=$APP_TOKEN
