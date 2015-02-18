#!/bin/bash
if [ $# -ne 2 ]; then
    echo "usage: <hex seed> <index>"
    exit 1
fi

private=$(sx hd-seed $1 | sx hd-priv 0 | sx hd-priv 0 | sx hd-priv $2)
echo $private | sx hd-to-wif
echo $private | sx hd-to-address

# Only generate a testnet address if the necessary bitcoin-bash-tools
# are available (https://github.com/grondilu/bitcoin-bash-tools):
if [ -f bitcoin.sh ]; then
    . bitcoin.sh
    decoded=$(decodeBase58 $(echo $private | sx hd-to-address))
    hexToAddress ${decoded:2:40} 6F
fi
