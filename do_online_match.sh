#!/bin/bash

#ip=$2
#[ "$ip" = "" ] && ip=213.3.30.106
[ "$ip" = "" ] && ip=72.44.46.68
bot=$1
#port=$3
#[ "$port" = "" ] && port=9999
[ "$port" = "" ] && port=995
nick=$2
[ "$nick" = "" ] && nick=albert

while true; do
	./tcp $ip $port $nick $bot #| grep "against";
	#sleep 5;
done
