#!/bin/bash

FILE="../scripts/factory.json"

ROLE=$(cat role_id)
SECRET=$(cat secret_id)
CA_PEM=$(cat certs/ca.pem | sed ':a;N;$!ba;s/\n/\\n/g')

#IP_LINE=$(ifconfig | grep -E '192.168.[01][0-9]*')
IP=$(ifconfig | grep -E '192.168.[01][0-9]*' | sed -E -e 's/.*inet //' -e 's/ .*//')
URL="http://$IP:8200"

echo $ROLE
echo $SECRET

NEW=$(jq ".vault_role=\"$ROLE\" | .vault_secret=\"$SECRET\" | .vault_url=\"$URL\" | .ca_pem=\"$CA_PEM\"" $FILE)
echo $NEW

echo $NEW > $FILE

