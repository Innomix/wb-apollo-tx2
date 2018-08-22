#!/bin/bash

exec > /tmp/start.out 2>&1;set -x

ethtool -s eth0 speed 100 duplex full

#curl --request PUT --url http://127.0.0.1:5000/map

map_path="/opt/apollo/uploads/cmap.stcm"

if [ ! -f "$map_path" ]; then
    echo "no default map" > /tmp/apollo.log
    exit 0
fi

while true
do
    /opt/apollo/bin/apollod --loadmap $map_path -x 0 -y 0
    if [ $? -eq 0 ]; then
        exit 0
    fi
    sleep 3
done

exit 0
