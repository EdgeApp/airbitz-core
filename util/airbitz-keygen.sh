#!/bin/bash
if [ $# -ne 2 ]; then
    echo "usage: <hex seed> <index>"
    exit 1
fi

private=$(bx hd-new $1 | bx hd-private -i 0 | bx hd-private -i 0 | bx hd-private -i $2)
bx hd-to-wif $private
bx hd-to-address $private

# Only generate a testnet address if the necessary bitcoin-bash-tools
# are available (https://github.com/grondilu/bitcoin-bash-tools):
if [ -f bitcoin.sh ]; then
    . bitcoin.sh
    decoded=$(decodeBase58 $(bx hd-to-address $private))
    hexToAddress ${decoded:2:40} 6F
fi
