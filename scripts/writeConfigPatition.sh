#!/bin/bash

#SCRIPT_PATH="$(readlink -f "$0")"
SCRIPT_PATH="$(dirname "$(readlink -f "$0")")"

echo $SCRIPT_PATH

parttool.py write_partition --partition-name=config --input "$SCRIPT_PATH/factory.json"

