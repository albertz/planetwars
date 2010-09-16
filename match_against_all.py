#!/usr/bin/python

import os, sys
import random, shlex
from pymatch import *

bot1 = sys.argv[1]

for bot2 in bots:
	print bot2, ":",
	wins = [ match(bot1, bot2, m) for m in maps[10:20] ]
	print "wins =", len(filter(lambda x: x == 1, wins)),
	print ", losts =", len(filter(lambda x: x == 2, wins)),
	print ", draws =", len(filter(lambda x: x == 0, wins))
