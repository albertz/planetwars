
ip=$1
[ "$ip" = "" ] && ip=213.3.30.106
bot=./simpleRankingBot
port=$2
[ "$port" = "" ] && port=9999

while true; do
	./tcp $ip $port albert $bot | grep "against";
	sleep 5;
done
