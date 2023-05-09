#!/bin/bash


#grep -v 'I .*' | sed -e 's/deg://' -e 's/.*RUN: //' -e 's/s: //' -e 's/ps: //' -e 's/sa: //' -e 's/(/,/' -e 's/)//'

grep -v 'I .*' | sed -E -e 's/.*RUN: //' -e 's/[a-z]+: *//g' -e 's/\(/,/' -e 's/\)//'


