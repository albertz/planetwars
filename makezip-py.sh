#!/bin/bash

botpy=$1
[ "$botpy" = "" ] && {
	echo "give me a bot"
	exit 1
}

rm -r albert.zip albertzip
mkdir albertzip

cp $botpy albertzip/MyBot.py
cp {PlanetWars,utils}.py albertzip/

cd albertzip
zip -9 albert.zip *
mv albert.zip ..
cd ..

rm -r albertzip
