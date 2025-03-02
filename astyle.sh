#!/bin/bash 
#
astyle --suffix=none --formatted --options=.astyle --recursive "main/*.c,.h" "components/*.c,.h"
