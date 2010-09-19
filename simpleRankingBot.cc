#include <iostream>
#include <algorithm>
#include <vector>
#include <map>
#include <cmath>
#include <string>
#include <set>
#include "game.h"
using namespace std;

typedef std::vector<Planet> Ps;
typedef std::vector<Fleet> Fs;
typedef std::vector<int> Is;
typedef Ps::iterator Pit;
typedef Fs::iterator Fit;
typedef Is::iterator Iit;

static Game _pw;

float rankPlanet(Planet& p);

struct FleetOrderByTurnsRemaining {
	bool operator()(const Fleet& f1, const Fleet& f2) const {
		return f1.turnsRemaining < f2.turnsRemaining;
	}
};

template<typename T>
static void fillIterate(T& cont, int start, int end) {
	for(int i = start; i != end; ++i)
		cont.push_back(i);
}

struct D {
	// statics
	Ps planets;
	Ps myPlanets;
	
	// we manipulate them while giving orders
	Fs fleets;
	int myAvailableShipsNum;
	
	// those will be calculated somewhere else
	std::map<int,int> myAvailableShips;
	std::set<int> haveSentShipsToPlanet;
	
	void update(const std::string& map_data) {
		_pw.ParseGameState(map_data);
		planets = _pw.Planets();
		myPlanets = _pw.MyPlanets();
		fleets = _pw.Fleets();
		// TODO: what player order?
		sort(fleets.begin(), fleets.end(), FleetOrderByTurnsRemaining());
		myAvailableShipsNum = 0;
		myAvailableShips.clear();
		haveSentShipsToPlanet.clear();
	}
};

struct PlanetOrderByDistance {
	int planetDestId;
	PlanetOrderByDistance(int _planetDestId) : planetDestId(_planetDestId) {}
	
	bool operator()(int p1, int p2) const {
		return _pw.desc.Distance(p1, planetDestId) < _pw.desc.Distance(p2, planetDestId);
	}
};

static D pw;

#define MAXSIM 1000

struct RelevantPlanetState {
	int time;
	int owner;
	int ships;
	int shipsWeCanGiveAway;
};

RelevantPlanetState relevantPlanetState(const Planet& p) {
	RelevantPlanetState s;
	s.time = 0;
	s.ships = p.numShips;
	s.owner = p.owner;
	s.shipsWeCanGiveAway = s.ships;
	
	for(Fit f = pw.fleets.begin(); f != pw.fleets.end(); ++f) {
		if(f->destinationPlanet != p.planetId) continue; // not relevant

		int dt = f->turnsRemaining - s.time;
		s.time = f->turnsRemaining;
		if(s.owner > 0) s.ships += dt * p.growthRate;
		
		if(s.owner == f->owner)
			s.ships += f->numShips;
		else {
			if(s.owner == 1)
				s.shipsWeCanGiveAway = min(s.shipsWeCanGiveAway, s.ships - f->numShips);
			
			s.ships -= f->numShips;
			if(s.ships < -1) { // change owner
				s.ships *= -1;
				s.owner = f->owner;
			}
			else if(s.ships == -1)
				s.ships = 0; // seems this is a special rule?
			
		}
	}

	return s;
}

float averageDistToNotOwnedPlanets(const Planet& p) {
	int num = 0;
	float sum = 0;
	for(Pit i = pw.planets.begin(); i != pw.planets.end(); ++i) {
		if(i->owner == 1) continue;
		num++;
		sum += _pw.desc.Distance(p.planetId, i->planetId);
	}
	if(num > 0) return sum / float(num);
	return 0.0f;
}

#define SHIPSNEEDEDTOCONQUER 2

float rankPlanetPosition(const Planet& p) {
	return expf(-averageDistToNotOwnedPlanets(p) * 0.1f);	
}

float rankPlanet(const Planet& p) {
	RelevantPlanetState s = relevantPlanetState(p);
	Is planets; planets.reserve(_pw.NumPlanets());
	for(size_t i = 0; i < _pw.NumPlanets(); ++i) {
		if(_pw.state.planets[i].owner == 1)
			planets.push_back(i);
	}
	sort(planets.begin(), planets.end(), PlanetOrderByDistance(p.planetId));
	
	if(s.owner != 1) {
		int numShips = 0;
		int time = 0;
		for(Iit i = planets.begin(); i != planets.end(); ++i) {
			if(*i == p.planetId) continue; // it is not ours anymore
			int dist = _pw.desc.Distance(*i, p.planetId);
			if(dist > time) time = dist;
			numShips += pw.myAvailableShips[*i] - 1; // not all, keep one at least

			if(numShips >= s.ships + SHIPSNEEDEDTOCONQUER) break; // we have enough together
		}
		
		if(numShips < s.ships + SHIPSNEEDEDTOCONQUER) return -1000.0f; // we cannot get it at all right now
		
		return
		10.0f * float(p.growthRate) * 0.2f * (1.0f - float(s.ships + SHIPSNEEDEDTOCONQUER) / float(pw.myAvailableShipsNum)) +
		10.0f * expf(-float(time)*0.1f);
	}
	
	int numShipsShouldHave = int( float(p.growthRate) * 0.2f * float(pw.myAvailableShipsNum) / float(pw.myPlanets.size()) );
	
	// only send ships here if we really have enough
	if(s.ships < numShipsShouldHave)
		// very simple for now -- if we are closer to enemy/neutral, that's better. low priority though
		return
		rankPlanetPosition(p) * float(p.growthRate) * 0.2f;
	
	return 0.0f; // don't
}

int getBestPlanetByRank() { // only with rank > 0
	int bestP = -1;
	float bestR = 0.0f;
	for(size_t p = 0; p < _pw.NumPlanets(); ++p) {
		if(pw.haveSentShipsToPlanet.count(p) > 0) continue; // dont sent ships there again
		
		float r = rankPlanet(_pw.GetPlanet(p));
		if(r > bestR) {
			bestR = r;
			bestP = p;
		}
	}
	return bestP;
}

bool DoConquerPlanet(int p) {
	Is planets; planets.reserve(_pw.NumPlanets());
	for(size_t i = 0; i < _pw.NumPlanets(); ++i) {
		if(_pw.state.planets[i].owner == 1)
			planets.push_back(i);
	}
	sort(planets.begin(), planets.end(), PlanetOrderByDistance(p));
	
	RelevantPlanetState s = relevantPlanetState(_pw.GetPlanet(p));
	int neededShips =
		(s.owner != 1) ?
		(s.ships + SHIPSNEEDEDTOCONQUER) :
		((pw.myAvailableShipsNum - pw.myAvailableShips[p]) / 2); // "2" is just made up...
	
	if(neededShips <= 0)
		// seems that we don't have much ships available anymore
		return false;
	
	int numShips = 0;
	int numPlanets = 0;
	for(Iit i = planets.begin(); i != planets.end(); ++i) {
		if(*i == p) continue; // it's the planet itself
		numShips += pw.myAvailableShips[*i] - 1; // not all, keep one at least
		numPlanets++;		
		if(numShips >= neededShips) break; // we have enough together
	}	
	
	if(numShips < neededShips || numShips == 0)
		// this really should not happen. however, break out
		return false;
		
	for(Iit i = planets.begin(); i != planets.end(); ++i) {
		if(*i == p) continue; // it's the planet itself
		
		int sendShipsNum = std::min(pw.myAvailableShips[*i], neededShips);
		if(sendShipsNum <= 0) continue;
		
		_pw.IssueOrder(*i, p, sendShipsNum);
		int dist = _pw.desc.Distance(*i, p);
		pw.fleets.push_back( Fleet(1, sendShipsNum, *i, p, dist, dist) );
		
		pw.myAvailableShips[*i] -= sendShipsNum;
		pw.myAvailableShipsNum -= sendShipsNum;
		neededShips -= sendShipsNum;
		
		if(neededShips <= 0) break;
	}
	
	pw.haveSentShipsToPlanet.insert(p);
	return true;
}

void DoTurn() {
	// myAvailableShips* is 0 here
	// we keep so many ships so that the enemy does not take our planets away
	for(Pit p = pw.myPlanets.begin(); p != pw.myPlanets.end(); ++p) {
		RelevantPlanetState s = relevantPlanetState(*p);
		if(s.shipsWeCanGiveAway > 0)
			pw.myAvailableShips[p->planetId] = s.shipsWeCanGiveAway;
		else
			pw.myAvailableShips[p->planetId] = 0;
		pw.myAvailableShipsNum += pw.myAvailableShips[p->planetId];
	}
		
	int p = -1;
	while((p = getBestPlanetByRank()) >= 0)
		if(!DoConquerPlanet(p))
			return;
}

// This is just the main game loop that takes care of communicating with the
// game engine for you. You don't have to understand or change the code below.
int main(int argc, char *argv[]) {
	std::string current_line;
	std::string map_data;
	while (true) {
		int c = std::cin.get();
		if(c < 0) break;
		current_line += (char)(unsigned char)c;
		if (c == '\n') {
			if (current_line.length() >= 2 && current_line.substr(0, 2) == "go") {
				pw.update(map_data);
				map_data = "";
				DoTurn();
				_pw.FinishTurn();
			} else {
				map_data += current_line;
			}
			current_line = "";
		}
	}
	return 0;
}
