#!/bin/bash

# crontab:
# 1  *  *  *  *  root  /home/pi/wmr100-pi/wmr-watch.sh

WATCH_FILE="/tmp/wmr100.cycle"
SLEEP_TIME=20

c0=`cat $WATCH_FILE`
sleep($SLEEP_TIME)
c1=`cat $WATCH_FILE`

echo "c0:" $c0
echo "c1:" $c1

if ["$c0" == "$c1"]; then 
    echo "reboot"
    # reboot
fi

#.

