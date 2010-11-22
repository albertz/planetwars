#!/usr/bin/python

from PlanetWars import PlanetWars
from operator import *
from math import *
from itertools import *
from time import time


def objRepresentingArgs(obj):
	for attrib in obj.__dict__:
		if not attrib.startswith("_"):
			yield attrib

def copyAttributes(dst, src):
	for attrib in objRepresentingArgs(src):
		setattr(dst, attrib, getattr(src, attrib))

def initFromKWArgs(dst, args):
	for attrib in args:
		setattr(dst, attrib, args[attrib])
	

def handleBase(obj, base):
	if base:
		copyAttributes(obj, base)
		obj._base = base

def standardRepr(obj):
	str = obj.__class__.__name__ + "("
	attribs = map(lambda attrib: attrib + "=" + repr(getattr(obj, attrib)), objRepresentingArgs(obj))
	str += ", ".join(attribs) + ")"
	return str		


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
		

def entitiesForPlanet(state, planet):
	entities = map(Planet, ifilter(lambda p: p != planet, state.planets))
	for e in entities:
		dx = e._base._x - planet._x
		dy = e._base._y - planet._y
		e.dist = int(ceil(sqrt(dx * dx + dy * dy)))
		if e.owner == planet.owner:
			e.owner = 1
		elif 0 != e.owner < planet.owner:
			e.owner += 1		
	entities += map(Fleet, pw.Fleets())
	entities.sort(key = attrgetter("dist"))
	return entities


# create some general summed state
def sumState(state):
	pass

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

