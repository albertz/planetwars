#!/bin/bash

map=$3
[ "$map" = "" ] && map=maps/map1.txt
timeout=100000
maxturns=2000
logfile=log.txt
bot1=$1
bot2=$2

./planetwars/playnview $map $timeout $maxturns $logfile "$bot1" "$bot2"
