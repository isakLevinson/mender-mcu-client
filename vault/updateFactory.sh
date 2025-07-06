#!/bin/bash

FILE="../scripts/factory.json"

ROLE=$(cat role_id)
SECRET=$(cat secret_id)
CA_PEM=$(cat certs/ca.pem | sed ':a;N;$!ba;s/\n/\\n/g')

echo $ROLE
echo $SECRET

NEW=$(jq ".vault_role=\"$ROLE\" | .vault_secret=\"$SECRET\" | .ca_pem=\"$CA_PEM\"" $FILE)
echo $NEW

echo $NEW > $FILE

