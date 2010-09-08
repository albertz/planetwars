#!/bin/zsh

for m in maps/*.txt; do
echo -n "$m: "
java -jar ./tools/PlayGame.jar $m 1000 1000 log.txt "./MyBot" \
"java -jar $1" 2>&1 >/dev/null | grep "Wins"
done

