#!/usr/bin/python

from glob import glob
from subprocess import *
import os, sys
import random, shlex

engineBin = "./planetwars/playgame"
botTimeout = 100000
maxTurns = 200 # like official
logFile = "log.txt"

bots = ["./" + b for b in glob("*Bot")]
bots += [ "./" + b for b in filter(lambda x: not "." in x, glob("planetwars/Bot*")) ]
#bots += [ "java -jar " + b for b in glob("example_bots/*.jar") ]
maps = glob("maps/*.txt")

def startMatch(bot1, bot2, m):
	cmd = (engineBin, m, str(botTimeout), str(maxTurns), logFile, bot1, bot2)
	p = Popen(cmd, stdout=open("/dev/null"), stderr=PIPE)
	return p

def winningPlayer(engineStream):
	l = filter(lambda x: "Win" in x, engineStream.xreadlines())
	if len(l) >= 1:
		return int(l[0].split()[1]) # eg. "Player 1 Wins"
	else:
		return 0

def match(bot1, bot2, m):
	p = startMatch(bot1, bot2, m)
	return winningPlayer(p.stderr)
