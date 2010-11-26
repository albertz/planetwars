#!/usr/bin/python

from PlanetWars import *
from operator import *
from math import *
from itertools import *
from functools import *
from time import time
from utils import *
import sys
import random


state = None


def evalPlayerState(planets, owner):
	prod = growthRateSum(filterPlanets(planets, owner=owner))
	#ships = shipsSum(filterPlanets(planets, owner=owner))
	return prod

def evalState(s):
	planets = futurePlanets(s)
	return evalPlayerState(planets,1) - evalPlayerState(planets,2)
	
def evalOrders(orders):
	s = State()
	s.planets = map(Planet, state.planets)
	s.fleets = list(state.fleets)
	for source,dest,num_ships in orders:
		if s.planets[source].owner != 1: continue
		if source == dest: continue
		if num_ships > s.planets[source].shipNum: continue
		if num_ships <= 0: continue
		s.planets[source].shipNum -= num_ships
		f = Fleet(
			owner = 1,
			dist = planetDist(s.planets[source], s.planets[dest]),
			source = source,
			dest = dest,
			shipNum = num_ships)
		s.fleets += [f]
	return evalState(s)
	
def play():
	global state
	state = State.FromGlobal()
	planets = map(Planet, state.planets)
	myplanets = filter(lambda p: p.owner == 1, planets)
	notmyplanets = filter(lambda p: p.owner != 1, planets)
	enemyplanets = filter(lambda p: p.owner >= 2, planets)
	ownedplanets = filter(lambda p: p.owner > 0, planets)
	
	for p in notmyplanets:
		p.closest = min(izip(ownedplanets, imap(partial(planetDist, p), ownedplanets)), key = itemgetter(1))

	myclosestplanets = filter(lambda p: p.closest[0].owner == 1, notmyplanets)
	myclosestplanets.sort(key = lambda p: p.closest[1])
	myclosestplanets = map(lambda p: futurePlanet(p, state.fleets, planetDist(p, p.closest[0])), myclosestplanets)

	orders = []
	for p in notmyplanets:
		for q in enemyplanets:
			if q == p: continue
			if planetDist(p,q) < p.closest[1]:
				p.shipNum += q.shipNum
				p.shipNum += (p.closest[1] - planetDist(p,q)) * q.growthRate

	for p in notmyplanets:
		shipNum = p.shipNum + 1
		orders += [(p.closest[0]._planet_id, p._planet_id, shipNum)]
		p.closest[0].shipNum -= shipNum
	
	requirements = [] # (planet, time, shipnum)
	for p in myplanets:
		if p.shipNum < 0:
			requirements += [(p._planet_id, 0, -p.shipNum)]
			p.shipNum = 0
	
	for p in myplanets:
		fleets = filter(lambda f: f.dest == p._planet_id, state.fleets)
		fleets = groupFleets(fleets)
		for time,fs in fleets:
			for f in fs:
				if f.owner == 1: p.shipNum += f.shipNum
				else: p.shipNum -= f.shipNum
			if p.shipNum < 0:
				requirements += [(p._planet_id, time, -p.shipNum)]
				p.shipNum = 0
	
	requirements.sort(key = itemgetter(1))
	requirements.sort(key = itemgetter(0))
	
	for pid,time,shipNum in requirements:
		for p in myplanets:
			if time == 0 or planetDist(p, planets[pid]) == time:
				while p.shipNum > 0:
					orderShipNum = min(p.shipNum, 2)
					#orderShipNum = min(p.shipNum, shipNum)
					orders += [(p._planet_id, pid, orderShipNum)]
					p.shipNum -= orderShipNum
					shipNum -= orderShipNum
			if shipNum <= 0: break
	
	bestOrders, bestOrdersEval = list(orders), evalOrders(orders)
	for i in xrange(50):
		random.shuffle(orders)
		value = evalOrders(orders)
		#print "iter", i, ":", value
		if value > bestOrdersEval:
			bestOrders,bestOrdersEval = list(orders), evalOrders(orders)
			print "iter", i, ":", bestOrdersEval
			
	return bestOrders
	
	
def DoTurn(pw):
	orders = play()
	for source,dest,num_ships in orders:
		if state.planets[source].owner != 1: continue
		if source == dest: continue
		if num_ships > state.planets[source].shipNum: continue
		if num_ships <= 0: continue
		#num_ships = min(state.planets[source].shipNum, num_ships)
		pw.IssueOrder(source, dest, num_ships)
		state.planets[source].shipNum -= num_ships


def main():
	while True:
		pw = readNextGameState()
		DoTurn(pw)
		pw.FinishTurn()


if __name__ == '__main__':
  try:
    import psyco
    psyco.full()
  except ImportError:
    pass
  try:
    main()
  except KeyboardInterrupt:
    print 'ctrl-c, leaving ...'

