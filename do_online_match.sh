
ip=$1
[ $ip = "" ] && ip=213.3.30.106
bot=./simpleRankingBot

while true; do
	./tcp $ip 19999 albert $bot | grep "against";
	sleep 5;
done
