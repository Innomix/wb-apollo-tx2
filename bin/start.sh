#!/bin/bash

exec > /tmp/start.out 2>&1;set -x

sleep 10
#curl --request PUT --url http://127.0.0.1:5000/map

while true
do
    /opt/apollo/apollod --loadmap /opt/apollo/uploads/cmap.stcm -x 0 -y 0
    if [ $? -eq 0 ]; then
        exit 0
    fi
    sleep 3
done

exit 0
