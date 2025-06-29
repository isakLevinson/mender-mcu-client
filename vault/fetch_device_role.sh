export VAULT_ADDR=http://127.0.0.1:8200

ROLE_FILE="./role_id"
SECRET_FILE="./secret_id"

# Role ID (static)
ROLE_ID=$(vault read -format=json auth/approle/role/my-app-role/role-id | jq -r .data.role_id)

# Secret ID (dynamic, like a password)
SECRET_ID=$(vault write -f -format=json auth/approle/role/my-app-role/secret-id | jq -r .data.secret_id)

echo "ROLE=$ROLE_ID"
echo "SECRET=$SECRET_ID"

echo $ROLE_ID > $ROLE_FILE
echo $SECRET_ID > $SECRET_FILE

chmod 600 "$ROLE_FILE" "$SECRET_FILE"
