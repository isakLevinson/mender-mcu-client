#!/bin/bash

export VAULT_ADDR=http://127.0.0.1:8200

ROLE_ID=$(cat ./role_id)
SECRET_ID=$(cat ./secret_id)
APP_TOKEN_FILE="./app_token"

# Make sure ROLE_ID and SECRET_ID are already exported or defined above this

# Get the token and write it to file
#vault write -format=json auth/approle/login role_id="$ROLE_ID" secret_id="$SECRET_ID" | jq -r .auth.client_token | tee "$APP_TOKEN_FILE"
#vault write -format=json auth/approle/login role_id="$ROLE_ID" secret_id="$SECRET_ID" | jq -r .auth.client_token

JSON=$(echo "{\"role_id\": \"$ROLE_ID\", \"secret_id\": \"$SECRET_ID\"}" | sed 's/ //g')
echo "JSON=$JSON"

APP_TOKEN=$(curl -s --request POST --data "$JSON" http://127.0.0.1:8200/v1/auth/approle/login | jq -r .auth.client_token)

# Now export the token from the file
echo "$APP_TOKEN" > "$APP_TOKEN_FILE"

echo TOKEN=$APP_TOKEN
