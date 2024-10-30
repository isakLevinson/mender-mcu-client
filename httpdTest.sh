#!/bin/bash

curl -k -H "Content-Type: app" -d "{cert: abcdefg}" -X POST https://192.168.1.234/config
