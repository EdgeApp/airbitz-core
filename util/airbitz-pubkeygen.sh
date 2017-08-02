#!/bin/bash
set -e
if [ $# -ne 2 ]; then
    echo "usage: <hex seed> <max-index>"
    exit 1
fi

public=$(bx hd-public -i 0 $1)

for index in $(seq 0 $2)
do
    address=$(bx hd-public -i $index $public | bx hd-to-address)
    echo $address '#'$index
done
