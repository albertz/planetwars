#!/usr/bin/python

from PlanetWars import *
import PlanetWars
from operator import *
from math import *
from itertools import *
from functools import *
from time import time
from utils import *
import sys
import random


Debug = True
if Debug:
	from pylab import ion, ioff, figure, draw, contourf, clf, show, hold, plot

	def plotPoints(points):
		figure(1)
		ioff()  # interactive graphics off
		clf()   # clear the plot
		hold(True) # overplot on
		for x,y in points:
			plot(x,y,'o')
		#if out.max()!=out.min():  # safety check against flat field
		#	contourf(X, Y, out)   # plot the contour
		ion()   # interactive graphics on
		draw()  # update the plot		



def randomPlanetSet(centralPlanet, planets):
	mindist = min(imap(partial(planetDist, centralPlanet), ifilter(lambda p: p != centralPlanet, planets)))
	centralPlanets = [centralPlanet]
	dists = []
	for p in planets:
		if p == centralPlanet: continue
		if p.owner != centralPlanet.owner: continue
		dist = planetDist(centralPlanet, p)
		rnddist = abs(random.gauss(0.0, mindist * 1.0))
		if dist >= rnddist: continue
		centralPlanets += [p]
		dists += [dist]
	if len(dists) > 0:
		distAverage = float(sum(dists)) / len(dists)
	else:
		distAverage = 0.0
	return centralPlanets, distAverage


def vecValues(v):
	if hasattr(v, "_x") and hasattr(v, "_y"):
		return (v._x, v._y)
	return v
	
def vecDist(v1, v2):
	x1,y1 = v1
	x2,y2 = v2
	return hypot(x1-x2, y1-y2)
	
def vecAdd(v1, v2): return tuple(imap(add, v1, v2))
def vecMul(v1, f): return tuple(imap(mul, v1, repeat(f)))


def vecMerge(base, baseNum, vec):
	v = vecAdd(vecMul(base, baseNum), vec)
	baseNum += 1
	v = vecMul(v, 1.0 / baseNum)
	return v
	

def selectNearestPlanets(basePlanet, distPlanetCenter, planets):
	base = (basePlanet._x, basePlanet._y)
	distCenter = (distPlanetCenter._x, distPlanetCenter._y)
	minDist = vecDist(base, distCenter) / 2.0
	maxDist = vecDist(base, distCenter) * 4.0
	planets = list(planets)
	planets.sort(key = partial(planetDist, basePlanet))
	nearestPlanets = [basePlanet]
	for p in planets:
		if p == basePlanet: continue
		if p.owner != basePlanet.owner: continue
		x,y = p._x, p._y
		if vecDist((basePlanet._x, basePlanet._y), (x,y)) < minDist: continue
		#if vecDist(base, (x,y)) > maxDist: continue
		newBase = vecMerge(base, len(nearestPlanets), (x,y))
		if vecDist((basePlanet._x, basePlanet._y), newBase) < minDist: continue
		nearestPlanets += [p]
		base = newBase
	return nearestPlanets, base

def planetsAvgPos(planets):
	pos = (0,0)
	num = 0
	for p in planets:
		pos = vecMerge(pos, num, (p._x, p._y))
		num += 1
	return pos

	
# create some general summed state
def sumState(state):
	summedState = State()
	centralPlanet = random.choice(filter(lambda p: p.owner > 0, state.planets))
	#centralPlanet = random.choice(filter(lambda p: p.owner == 1, state.planets))
	centralPlanets,distAverage = randomPlanetSet(centralPlanet, state.planets)
	summedState.variance = distAverage

	planetPartition = []
	restPlanets = set(state.planets) - set(centralPlanets)	
	while len(restPlanets) > 0:
		p = random.choice(tuple(restPlanets))
		pGroup,_ = selectNearestPlanets(p, centralPlanet, restPlanets)
		planetPartition += [pGroup]
		restPlanets -= set(pGroup)
	
	class SummedPlanet(Planet):
		def __repr__(self):
			return "{" + ",".join(imap(str, self.planetIds)) + "}"
			
	oldPlanetIdToNew = {}
	for pGroup in chain([centralPlanets], planetPartition):
		p = SummedPlanet()
		p._planet_id = len(summedState.planets)
		p.owner = pGroup[0].owner
		p.shipNum = sum(imap(attrgetter("shipNum"), pGroup))
		p._x, p._y = planetsAvgPos(pGroup)
		p.growthRate = sum(imap(attrgetter("growthRate"), pGroup))
		p.planets = pGroup
		p.planetIds = set(imap(attrgetter("_planet_id"), pGroup))
		for oldid in p.planetIds: oldPlanetIdToNew[oldid] = p._planet_id
		summedState.planets += [p] 	
	summedState.centralPlanet = summedState.planets[0]
	
	fleets = []
	for f in state.fleets:
		newf = Fleet()
		newf.owner = f.owner
		newf.shipNum = f.shipNum
		newf.dist = f.dist
		newf.source = oldPlanetIdToNew[f.source]
		newf.dest = oldPlanetIdToNew[f.dest]
		fleets += [newf]	
	summedState.fleets = fleets
	
	return summedState


def ordersForPlanet(planet, distVariance, entities):
	myShipNum = planet.shipNum
	orders = []
	for e in entities:
		if e.owner >= 2 and isinstance(e, Fleet):
			myShipNum -= e.shipNum
	for e in entities:
		if e.owner == 1:
			if myShipNum < 0 < e.shipNum and isinstance(e, Planet):
				shipNum = min(-myShipNum, e.shipNum)
				orders += [(e._planet_id, -shipNum)]
			# TODO: maybe there are more cases where it makes sense to send ships here
		else: # neutral or enemy
			if isinstance(e, Planet):
				shipNum = e.shipNum + 1
				if e.owner > 0: shipNum += distVariance * e.growthRate
				if shipNum <= myShipNum:
					orders += [(e._planet_id, shipNum)]
					myShipNum -= shipNum
	return orders

def specializeOrders(realState, summedState, orders):
	def planetClosestDest(planet, dstplanets):
		dstplanets = set(dstplanets)
		for p,d in planet._distances:
			if p in dstplanets:
				return p,d
		assert False

	for srcplanet in realState.planets:
		distances = []
		for dstplanet in realState.planets:
			if dstplanet == srcplanet: continue
			distances += [(dstplanet,planetDist(srcplanet,dstplanet))]
		distances.sort(key = itemgetter(1))
		srcplanet._distances = distances
	
	def planetsByDist(summedPlanet, summedDestPlanet):	
		planets = [(p,) + planetClosestDest(p, summedDestPlanet.planets) for p in summedPlanet.planets]
		planets.sort(key = itemgetter(2))
		return map(itemgetter(0,1), planets)
	
	realOrders = []
	for srcplanet,dstplanet,shipNum in orders:
		if shipNum <= 0: continue
		srcplanet = summedState.planets[srcplanet]
		dstplanet = summedState.planets[dstplanet]
		for srcplanet,dstplanet in planetsByDist(srcplanet,dstplanet):
			if srcplanet.shipNum < 0: continue # probably does not happen but maybe in the future. just to be sure, just catch it here
			n = min(srcplanet.shipNum, shipNum)
			realOrders += [(srcplanet._planet_id, dstplanet._planet_id, n)]
			shipNum -= n
			if shipNum <= 0: break
			
	return realOrders


def nextState(state, orders):
	state = state.deepCopy()
	for source,dest,num_ships in orders:
		if num_ships == 0: continue
		f = Fleet()
		f.time = 0
		f.owner = state.planets[source].owner
		f.shipNum = num_ships
		f.source = source
		f.dest = dest
		state.fleets += [f]
	
	# merge fleets
	fleets = {} # index: (time,src,dst)
	for f in state.fleets:
		if f.time > 0 or f.source < f.dest:
			fi,num_ships = (f.time, f.source, f.dest), f.shipNum
		elif f.source > f.dest:
			fi,num_ships = (f.time, f.dest, f.source), -f.shipNum
		else:
			assert False
			continue # we can safely ignore that fleet. in fact, this should not happen at all
		if not fi in fleets: fleets[fi] = 0
		fleets[fi] += num_ships
	
	def makeFleet(fi):
		(time, source, dest), num_ships = fi
		f = Fleet()
		f.dist = planetDist(state.planets[source], state.planets[dest])
		f.time = time
		if time == 0 and num_ships < 0:
			source,dest = dest,source
			num_ships *= -1
		f.owner = state.planets[source].owner
		f.source = source
		f.dest = dest
		f.shipNum = num_ships
		return f
	state.fleets = map(makeFleet, fleets.iteritems())

	return state

def evalState(state):
	planets = futurePlanets(state)
	prod1 = growthRateSum(filterPlanets(planets, owner=1))
	prod2 = growthRateSum(filterPlanets(planets, owner=2))
	return prod1 - prod2
	
def ordersFromState(state):
	orders = []
	for f in state.fleets:
		if f.time > 0: continue
		orders += [(f.source,f.dest,f.shipNum)]
	orders.sort(key = lambda (src,dst,_): planetDist(state.planets[src], state.planets[dst]))
	return orders


initialState = None

def play():
	t = time()
	MaxLoops = 100
	
	global initialState
	initialState = State.FromGlobal()
	if isempty(ifilter(lambda p: p.owner == 1, initialState.planets)): return []
	state = initialState
	bestState,bestEval = state,evalState(state)
	print "initial, eval:", bestEval
	
	c = 0
	while True:
		summedState = sumState(state)
		centralPlanet = summedState.centralPlanet
		orders = ordersForPlanet(centralPlanet, summedState.variance, entitiesForPlanet(summedState, centralPlanet))
		orders = [(summedState.centralPlanet._planet_id,dest,shipNum) for (dest,shipNum) in orders]
		realOrders = specializeOrders(state, summedState, orders)
		newState = nextState(state, realOrders)
		eval = evalState(newState)
		
		if eval > bestEval:
			print "iter", c, ", eval:", eval
			state = newState
			bestState,bestEval = state,eval
			
		if time() - t > 1.0: break
		c += 1
		if c > MaxLoops > 0: break
		
	return ordersFromState(bestState)
	
	


def DoTurn(pw):
	orders = play()
	state = initialState
	for source,dest,num_ships in orders:
		if state.planets[source].owner != 1: continue
		if num_ships > state.planets[source].shipNum: continue
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

