openssl verify -CAfile ca.pem -untrusted intermediate.pem "$1"
