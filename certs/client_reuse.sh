openssl s_client -connect 192.168.101.73:443 -cert client.crt -key client.key -CAfile ca.crt -sess_in session.pem


