#!/bin/bash

map=$3
[ "$map" = "" ] && map=maps/map1.txt
timeout=100000
maxturns=200
logfile=log.txt
bot1=$1
bot2=$2

./planetwars/playgame $map $timeout $maxturns $logfile "$bot1" "$bot2" \
| ./planetwars/showgame
