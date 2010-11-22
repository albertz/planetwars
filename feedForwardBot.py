#!/usr/bin/python

from PlanetWars import PlanetWars
from operator import *
from math import *
from itertools import *
from functools import *
from time import time
from utils import *
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



class Base:
	__repr__ = standardRepr
	
# either a fleet or a planet
class Entity(Base):
	def __init__(self):
		self.owner = None
		self.shipNum = None
		self.dist = None

class Fleet(Entity):
	def __init__(self, base = None):
		Entity.__init__(self)
		self.source = None
		handleBase(self, base)
		
class Planet(Entity):
	def __init__(self, base = None):
		Entity.__init__(self)
		self.growthRate = None
		handleBase(self, base)
		if base and hasattr(base, "_x") and hasattr(base, "_y"):
			self._x,self._y = base._x,base._y


def planetDist(p1, p2):
	dx = p1._x - p2._x
	dy = p1._y - p2._y
	return int(ceil(sqrt(dx * dx + dy * dy)))


def entitiesForPlanet(state, planet):
	entities = []

	planets = {}
	for p in state.planets:
		if p == planet: continue
		e = Planet(p)
		entities += [e]
		planets[p._planet_id] = e
		e.dist = planetDist(e, planet)
		if e.owner == planet.owner:
			e.owner = 1
		elif 0 != e.owner < planet.owner:
			e.owner += 1

	for f in state.fleets:
		if f.dest == planet._planet_id:
			e = Fleet(f)
			entities += [e]
		else:
			# NOTE: We ignore the time when this fleet arrives.
			# I think this should be good enough.
			# If not, more complicated calculations are possible here.
			destPlanet = planets[planet._planet_id]
			if f.owner == destPlanet.owner:
				destPlanet.shipNum += f.shipNum
			else:
				destPlanet.shipNum -= f.shipNum				
				
	entities.sort(key = attrgetter("dist"))
	return entities


class State:
	def __init__(self):
		self.planets = []
		self.fleets = []

	@staticmethod
	def FromGlobal():
		state = State()
		state.planets = pw.Planets()
		state.fleets = pw.Fleets()
		return state
	
	
def randomPlanetSet(centralPlanet, planets):
	mindist = min(imap(partial(planetDist, centralPlanet), planets))
	centralPlanets = [centralPlanet]
	dists = []
	for p in state.planets:
		if p == centralPlanet: continue
		if p.owner != centralPlanet.owner: continue
		dist = planetDist(centralPlanet, p)
		if dist >= abs(random.gauss(0, mindist)): continue
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
	x2,v2 = v2
	return hypot(x1-x2, y1-y2)

def vecAdd(v1, v2): return tuple(map(add, izip(v1,v2)))
def vecMul(v1, f): return tuple(map(mul, izip(v1,repeat(f))))

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
		if p.owner != basePlanet: continue
		x,y = p._x, p._y
		if vecDist(base, (x,y)) > maxDist: continue
		newBase = vecMerge(base, len(nearestPlanets), (x,y))
		if vecDist(base, newBase) < minDist: continue
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
	centralPlanet = random.choice(state.planets)
	centralPlanets,distAverage = randomPlanetSet(centralPlanet, state.planets)

	planetPartition = []
	restPlanets = set(state.planets) - set(centralPlanets)	
	while len(restPlanets) > 0:
		p = random.choice(restPlanets)
		pGroup,_ = selectNearestPlanets(p, centralPlanet, restPlanets)
		planetPartition += [pGroup]
		restPlanets -= set(pGroup)
	

	oldPlanetIdToNew = {}
	for pGroup in chain([centralPlanets], planetPartition):
		p = Planet()
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
		newf.source = oldPlanetIdToNew[f._source_planet]
		newf.dest = oldPlanetIdToNew[f._destination_planet]
		fleets += [newf]	
	summedState.fleets = fleets
	
	return summedState


def ordersForPlanet(entities):
	pass

def specializeOrders(summedState, orders):
	pass

def nextState(state, orders):
	pass

def evalState(state):
	pass

def ordersFromStateDiff(baseState, state):
	pass
	

def play():
	t = time()
	
	initialState = State.FromGlobal()
	state = initialState
	bestState,bestEval = state,0
	
	while True:
		summedState = sumState(state)
		centralPlanet = summedState.centralPlanet
		orders = ordersForPlanet(entitiesForPlanet(summedState, centralPlanet))
		realOrders = specializeOrders(summedState, orders)
		state = nextState(state, realOrders)
		eval = evalState(state)
		if eval > bestEval:
			bestState,bestEval = state,eval
		
		if time() - t > 1.0: break
	
	return ordersFromStateDiff(initialState, bestState)
	
	


def DoTurn(pw):
	orders = play()
	for source,dest,num_ships in orders:
		pw.IssueOrder(source, dest, num_ships)


pw = None
if Debug: pw = PlanetWars(open("maps/map50.txt").read())
	

def main():
  global pw
  map_data = ''
  while(True):
    current_line = raw_input()
    if len(current_line) >= 2 and current_line.startswith("go"):
      pw = PlanetWars(map_data)
      DoTurn(pw)
      pw.FinishTurn()
      map_data = ''
    else:
      map_data += current_line + '\n'


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
