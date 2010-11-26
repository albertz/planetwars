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

def validOrders(orders):
	validOrders = []
	planets = map(Planet, state.planets)
	for source,dest,num_ships in orders:
		if planets[source].owner != 1: continue
		if source == dest: continue
		if num_ships > planets[source].shipNum: continue
		if num_ships <= 0: continue
		planets[source].shipNum -= num_ships
		validOrders += [(source,dest,num_ships)]
	return validOrders, planets
	

def mergeOrders(orders):
	o = {} # (src,dst) -> shipNum
	for src,dst,shipNum in orders:
		if src < dst:
			key,shipNum = (src, dst), shipNum
		elif src > dst:
			key,shipNum = (dst, src), -shipNum
		else:
			assert False # this should not happen
		if not key in o: o[key] = 0
		o[key] += shipNum
	
	orders = []
	for (src,dst),shipNum in o.iteritems():
		if shipNum == 0: continue
		if shipNum < 0: src,dst,shipNum = dst,src,-shipNum
		orders += [(src,dst,shipNum)]
	
	return orders
	
def filterObsoleteOrders(orders):
	newOrders = []
	fleets = set() # (src,dst) pairs
	for f in state.fleets:
		fleets.add((f.source, f.dest))
	for src,dst,shipNum in orders:
		if state.planets[src].owner == state.planets[dst].owner and (dst,src) in fleets: continue
		newOrders += [(src,dst,shipNum)]
	return newOrders	

def ordersForReqs(requirements, planets):
	orders = []
	for pid,time,shipNum in requirements:
		for p in planets:
			if p.owner != 1: continue
			if p._planet_id == pid: continue
			if time == 0 or planetDist(p, planets[pid]) <= time:
				while p.shipNum > 0:
					orderShipNum = min(p.shipNum, 2)
					#orderShipNum = min(p.shipNum, shipNum)
					orders += [(p._planet_id, pid, orderShipNum)]
					p.shipNum -= orderShipNum
					shipNum -= orderShipNum
			if shipNum <= 0: break
	return orders
	
	
def play():
	global state
	state = State.FromGlobal()
	planets = map(Planet, state.planets)
	myplanets = filter(lambda p: p.owner == 1, planets)
	if len(myplanets) == 0: return []
	notmyplanets = filter(lambda p: p.owner != 1, planets)
	enemyplanets = filter(lambda p: p.owner >= 2, planets)
	ownedplanets = filter(lambda p: p.owner > 0, planets)
	
	for p in notmyplanets:
		p.closest = min(izip(myplanets, imap(partial(planetDist, p), myplanets)), key = itemgetter(1))

	myclosestplanets = filter(lambda p: p.closest[0].owner == 1, notmyplanets)
	myclosestplanets.sort(key = lambda p: p.closest[1])
	myclosestplanets = map(lambda p: futurePlanet(p, state.fleets, planetDist(p, p.closest[0])), myclosestplanets)

	baseorders = []
	for p in notmyplanets:
		for q in enemyplanets:
			if q == p: continue
			if planetDist(p,q) < p.closest[1]:
				p.shipNum += q.shipNum
				p.shipNum += (p.closest[1] - planetDist(p,q)) * q.growthRate

	for p in notmyplanets:
		shipNum = p.shipNum + 1
		baseorders += [(p.closest[0]._planet_id, p._planet_id, shipNum)]
		p.closest[0].shipNum -= shipNum
	
	baserequirements = [] # (planet, time, shipnum)
	for p in myplanets:
		if p.shipNum < 0:
			baserequirements += [(p._planet_id, 0, -p.shipNum)]
			p.shipNum = 0
	baserequirements.sort(key = itemgetter(1))
	baserequirements.sort(key = itemgetter(0))
	
	bestOrders, bestOrdersEval = None, None
	for c in xrange(50):
		random.shuffle(baseorders)
		orders,resultingPlanets = validOrders(baseorders)
		orders = multidict(izip(imap(itemgetter(0), orders), orders)) # planetid -> order
		
		availableShips = map(attrgetter("shipNum"), resultingPlanets)
		requirements = []
		for p in resultingPlanets:
			if p.owner != 1: continue
			fleets = filter(lambda f: f.dest == p._planet_id, state.fleets)
			fleets = groupFleets(fleets)
			lastTime = 0
			for time,fs in fleets:
				if time > lastTime:
					growPlanet(p, time - lastTime)
					lastTime = time
				shipNumSum = 0
				for f in fs:
					if f.owner == 1: shipNumSum += f.shipNum
					else: shipNumSum -= f.shipNum
				p.shipNum += shipNumSum
				if p.shipNum < 0:
					if p._planet_id in orders:
						for i in xrange(len(orders[p._planet_id])):
							orderShipNum = orders[p._planet_id][i][2]
							if orderShipNum >= -p.shipNum:
								orderShipNum += p.shipNum
								p.shipNum = 0
							else:
								p.shipNum += orderShipNum
								orderShipNum = 0
							orders[p._planet_id][i] = orders[p._planet_id][i][0:2] + (orderShipNum,)
							if p.shipNum >= 0: break
				availableShips[p._planet_id] = min(availableShips[p._planet_id], p.shipNum)
				if p.shipNum < 0:
					requirements += [(p._planet_id, time, -p.shipNum)]
					p.shipNum = 0
		for i,shipNum in izip(count(0), availableShips):
			resultingPlanets[i].shipNum = shipNum
		
		requirements.sort(key = itemgetter(1))
		requirements.sort(key = itemgetter(0))
		
		orders = list(chain(*orders.itervalues()))
		orders += ordersForReqs(requirements, resultingPlanets)
		orders = mergeOrders(orders)
		orders = filterObsoleteOrders(orders)
		
		random.shuffle(orders)
		orders += ordersForReqs(baserequirements, resultingPlanets)
		orders = mergeOrders(orders)
		orders = filterObsoleteOrders(orders)		
		orders,_ = validOrders(orders)

		value = evalOrders(orders)
		if bestOrders is None or value > bestOrdersEval:
			bestOrders,bestOrdersEval = list(orders), value
			print "iter", c, ":", bestOrdersEval
			
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

