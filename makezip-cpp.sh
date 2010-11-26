#!/bin/bash

botcpp=$1
[ "$botcpp" = "" ] && {
	echo "give me a bot"
	exit 1
}

rm -r albert.zip albertzip
mkdir albertzip

cp $botcpp albertzip/MyBot.cc
cp planetwars/{game,utils}.{h,cpp} albertzip/
cp planetwars/vec.h albertzip/
cp SimpleBimap.h albertzip/

cd albertzip
zip -9 albert.zip *
mv albert.zip ..
cd ..

rm -r albertzip
