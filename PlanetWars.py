#!/usr/bin/env python
#

import sys
from utils import *
from operator import *
from math import *
from itertools import *
from functools import *

stdout = sys.stdout
sys.stdout = sys.stderr




class Base:
	__repr__ = standardRepr
	

# either a fleet or a planet
class Entity(Base):
	def __init__(self):
		self.owner = None
		self.shipNum = None
		self.dist = None

class Fleet(Entity):
	def __init__(self, base = None, **kwargs):
		Entity.__init__(self)
		self.source = None
		handleBase(self, base)
		handleKWArgs(self, kwargs)
		
class Planet(Entity):
	def __init__(self, base = None, **kwargs):
		Entity.__init__(self)
		self.growthRate = None
		handleBase(self, base)
		if base and hasattr(base, "_x") and hasattr(base, "_y"):
			self._x,self._y = base._x,base._y
		if base and hasattr(base, "_planet_id"):
			self._planet_id = base._planet_id
		handleKWArgs(self, kwargs)


def planetDist(p1, p2):
	dx = p1._x - p2._x
	dy = p1._y - p2._y
	return int(ceil(sqrt(dx * dx + dy * dy)))


def translateOwnerId(myOwnerId, ownerId):
	if ownerId == myOwnerId:
		return 1
	elif 0 != ownerId < myOwnerId:
		return ownerId + 1
	else:
		return ownerId

def entitiesForPlanet(state, planet):
	entities = []

	planets = {} # planet id -> planet entitiy
	for p in state.planets:
		if p == planet: continue
		e = Planet(p)
		entities += [e]
		planets[p._planet_id] = e
		e._base = p
		e.dist = planetDist(e, planet)
		e.owner = translateOwnerId(planet.owner, p.owner)
		if e.owner > 0: e.shipNum += e.dist * e.growthRate

	for f in state.fleets:
		owner = translateOwnerId(planet.owner, f.owner)		
		if f.dest == planet._planet_id:
			e = Fleet(f)
			e._base = f
			e.owner = owner
			entities += [e]
		else:
			# NOTE: We ignore the time when this fleet arrives.
			# I think this should be good enough.
			# If not, more complicated calculations are possible here.
			destPlanet = planets[f.dest]
			if owner == destPlanet.owner:
				destPlanet.shipNum += f.shipNum
			else:
				destPlanet.shipNum -= f.shipNum				
				if destPlanet.shipNum < 0:
					destPlanet.shipNum *= -1
					destPlanet.owner = owner

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
	
	def deepCopy(self):
		state = State()
		state.planets = list(self.planets)
		for p,i in izip(state.planets,count(0)):
			p = Planet(p)
			p._planet_id = i
			state.planets[i] = p
		
		state.fleets = list(self.fleets)
		for f,i in izip(state.fleets,count(0)):
			f = Fleet(f)
			state.fleets[i] = f
		
		return state
	
	def __repr__(self):
		return "State(" + repr(self.planets) + ")"




class PlanetWars:
  def __init__(self, gameState):
    self._planets = []
    self._fleets = []
    self.ParseGameState(gameState)

  def NumPlanets(self):
    return len(self._planets)

  def GetPlanet(self, planet_id):
    return self._planets[planet_id]

  def NumFleets(self):
    return len(self._fleets)

  def GetFleet(self, fleet_id):
    return self._fleets[fleet_id]

  def Planets(self):
    return self._planets

  def MyPlanets(self):
    r = []
    for p in self._planets:
      if p.Owner() != 1:
        continue
      r.append(p)
    return r

  def NeutralPlanets(self):
    r = []
    for p in self._planets:
      if p.Owner() != 0:
        continue
      r.append(p)
    return r

  def EnemyPlanets(self):
    r = []
    for p in self._planets:
      if p.Owner() <= 1:
        continue
      r.append(p)
    return r

  def NotMyPlanets(self):
    r = []
    for p in self._planets:
      if p.Owner() == 1:
        continue
      r.append(p)
    return r

  def Fleets(self):
    return self._fleets

  def MyFleets(self):
    r = []
    for f in self._fleets:
      if f.Owner() != 1:
        continue
      r.append(f)
    return r

  def EnemyFleets(self):
    r = []
    for f in self._fleets:
      if f.Owner() <= 1:
        continue
      r.append(f)
    return r

  def ToString(self):
    s = ''
    for p in self._planets:
      s += "P %f %f %d %d %d\n" % \
       (p.X(), p.Y(), p.Owner(), p.NumShips(), p.GrowthRate())
    for f in self._fleets:
      s += "F %d %d %d %d %d %d\n" % \
       (f.Owner(), f.NumShips(), f.SourcePlanet(), f.DestinationPlanet(), \
        f.TotalTripLength(), f.TurnsRemaining())
    return s

  def Distance(self, source_planet, destination_planet):
    source = self._planets[source_planet]
    destination = self._planets[destination_planet]
    dx = source.X() - destination.X()
    dy = source.Y() - destination.Y()
    return int(ceil(sqrt(dx * dx + dy * dy)))

  def IssueOrder(self, source_planet, destination_planet, num_ships):
    stdout.write("%d %d %d\n" % \
     (source_planet, destination_planet, num_ships))
    stdout.flush()

  def IsAlive(self, player_id):
    for p in self._planets:
      if p.Owner() == player_id:
        return True
    for f in self._fleets:
      if f.Owner() == player_id:
        return True
    return False

  def ParseGameState(self, s):
    self._planets = []
    self._fleets = []
    lines = s.split("\n")
    planet_id = 0

    for line in lines:
      line = line.split("#")[0] # remove comments
      tokens = line.split(" ")
      if len(tokens) == 1:
        continue
      if tokens[0] == "P":
        if len(tokens) != 6:
          return 0
        p = Planet(_planet_id= planet_id, # The ID of this planet
                   owner= int(tokens[3]), # Owner
                   shipNum= int(tokens[4]), # Num ships
                   growthRate= int(tokens[5]), # Growth rate
                   _x = float(tokens[1]), # X
                   _y = float(tokens[2])) # Y
        planet_id += 1
        self._planets.append(p)
      elif tokens[0] == "F":
        if len(tokens) != 7:
          return 0
        f = Fleet(owner= int(tokens[1]), # Owner
                  shipNum= int(tokens[2]), # Num ships
                  source= int(tokens[3]), # Source
                  dest= int(tokens[4]), # Destination
                  dist= int(tokens[6]), # remaining time
                  time= int(tokens[5]) - int(tokens[6])) # time
        self._fleets.append(f)
      else:
        return 0
    return 1

  def FinishTurn(self):
    stdout.write("go\n")
    stdout.flush()


pw = None

def readNextGameState():
	global pw
	map_data = ''
	while(True):
		current_line = raw_input()
		if len(current_line) >= 2 and current_line.startswith("go"):
			pw = PlanetWars(map_data)
			return pw
		else:
			map_data += current_line + '\n'



# fleets should be pre-filtered to be in the right time-frame
def fightBattle(p, fleets):
	participants = [0,0,0]
	participants[p.owner] = p.shipNum
	
	for f in fleets:
		# no time check here. we already filtered them
		if f.dest == p._planet_id:
			participants[f.owner] += f.shipNum

	winner = (0,0) # (player,shipNum)
	second = (0,0)
	for player,ships in izip(count(0), participants):
		if ships > second[1]:
			if ships > winner[1]:
				second = winner
				winner = (player, ships)
			else:
				second = (player, ships)
	
	if winner[1] > second[1]:
		p.shipNum = winner[1] - second[1]
		p.owner = winner[0]
	else:
		p.shipNum = 0
	

def growPlanet(p, dt):
	if p.owner > 0:
		p.shipNum += p.growthRate * dt

def growPlanets(planets, dt):
	for p in planets: growPlanet(p, dt)

def futurePlanets(state):
	fleets = {} # remaining dist -> fleets
	for f in state.fleets:
		if not f.dist in fleets: fleets[f.dist] = []
		fleets[f.dist] += [f]	
	fleets = list(fleets.iteritems())
	fleets.sort()
	
	planets = list(imap(Planet, state.planets))
	lastTime = 0
	for time,fs in fleets:
		if time > lastTime:
			growPlanets(planets, time - lastTime)
			lastTime = time
		for p in planets:
			fightBattle(p, fs)

	return planets

def groupFleets(_fleets):
	fleets = {} # remaining time -> fleets
	for f in _fleets:
		if not f.dist in fleets: fleets[f.dist] = []
		fleets[f.dist] += [f]	
	fleets = list(fleets.iteritems())
	fleets.sort()
	return fleets

def futurePlanet(planet, fleets, endTime):
	fleets = groupFleets(fleets)
	planet = Planet(planet)
	lastTime = 0
	for time,fs in fleets:
		if time > endTime > 0: break
		if time > lastTime:
			growPlanet(planet, time - lastTime)
			lastTime = time
		fightBattle(planet, fs)

	return planet
	

def filterPlanets(planets, owner):
	return ifilter(lambda p: p.owner == owner, planets)
def growthRateSum(planets): return sum(imap(attrgetter("growthRate"), planets))
def shipsSum(planets): return sum(imap(attrgetter("shipNum"), planets))


