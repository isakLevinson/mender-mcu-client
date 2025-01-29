#!/bin/bash

# Start HTTPS server
openssl s_server -key ca_key.pem -cert ca_cert.pem -port 8070
