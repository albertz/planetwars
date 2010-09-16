#!/usr/bin/python

import os, sys
import random, shlex
from pymatch import *

bot1 = sys.argv[1]
mrange = (10,30)

for bot2 in bots:
	print os.path.basename(bot2), ":",
	matches = [ match(bot1, bot2, m) for m in maps[mrange[0]:mrange[1]] ]
	wins = len(filter(lambda x: x == 1, matches))
	losts = len(filter(lambda x: x > 1, matches))
	print str(wins) + "/" + str(wins + losts), "wins"
