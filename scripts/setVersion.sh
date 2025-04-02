#!/bin/bash
#
TAG=$(git tag -l --contains HEAD | sed -E 's/[a-z]*//')
HASH=$(git rev-parse HEAD | cut -b 1-8 )

#echo $TAG
#echo $HASH

if [ ! -z "$TAG" ]; then
	VER=$TAG
else
	VER=$HASH
fi

echo $VER

