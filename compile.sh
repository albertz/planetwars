#!/bin/zsh

mkdir -p build

CFLAGS=(-Wall -g -I planetwars -arch i386)

exec() {
	echo $argv[1,-1]
	$argv[@]
}

# $1 - target
# $2 - dep list
isUpToDate() {
	target=$1
	deplist=$2
	for d in $deplist; do
		[ $d -nt $ofile ] && return 1
	done
	return 0
}

# $1 - target
# $2 - depfile
isUpToDate_obj() {
	target=$1
	depfile=$2
	if [ -e $depfile ] && [ -e $target ]; then
		files=($(cat $depfile | cut -d ":" -f "2-" -))
		isUpToDate $target $files && return 0
	fi
	return 1
}

for f in *Bot.cc; do
	ofile="build/${f/.cc/.o}"
	depfile="build/${f/.cc/.d}"
	isUpToDate_obj $ofile $depfile && continue
	exec g++ $f $CFLAGS -c -o $ofile -MMD
done

for f in *Bot.cc; do
	ofile="build/${f/.cc/.o}"
	target="${f/.cc/}"
	deps=($ofile planetwars/utils.o planetwars/game.o)
	isUpToDate $target $deps && continue
	exec g++ $deps $CFLAGS -o $target
done
