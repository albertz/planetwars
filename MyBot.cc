#include <iostream>
#include <algorithm>
#include <vector>
#include <map>
#include <cmath>
#include <string>
#include "PlanetWars.h"
using namespace std;

typedef std::vector<Planet> Ps;
typedef std::vector<Fleet> Fs;
typedef std::vector<int> Is;
typedef Ps::iterator Pit;
typedef Fs::iterator Fit;
typedef Is::iterator Iit;

static PlanetWars _pw;

float rankPlanet(Planet& p);

struct FleetOrderByTurnsRemaining {
	bool operator()(const Fleet& f1, const Fleet& f2) const {
		return f1.TurnsRemaining() < f2.TurnsRemaining();
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
	std::map<int,int> myAvailableShips;
	
	void update(const std::string& map_data) {
		_pw = PlanetWars(map_data);
		planets = _pw.Planets();
		myPlanets = _pw.MyPlanets();
		fleets = _pw.Fleets();
		// TODO: what player order?
		sort(fleets.begin(), fleets.end(), FleetOrderByTurnsRemaining());
		myAvailableShipsNum = 0;
		myAvailableShips.clear();
		for(Pit p = myPlanets.begin(); p != myPlanets.end(); ++p) {
			myAvailableShipsNum += p->NumShips();
			myAvailableShips[p->PlanetID()] = p->NumShips();
		}
	}
};

struct PlanetOrderByDistance {
	int planetDestId;
	PlanetOrderByDistance(int _planetDestId) : planetDestId(_planetDestId) {}
	
	bool operator()(int p1, int p2) const {
		return _pw.Distance(p1, planetDestId) < _pw.Distance(p2, planetDestId);
	}
};

static D pw;

#define MAXSIM 1000

struct RelevantPlanetState {
	int time;
	int owner;
	int ships;
};

RelevantPlanetState relevantPlanetState(const Planet& p) {
	RelevantPlanetState s;
	s.time = 0;
	s.ships = p.NumShips();
	s.owner = p.Owner();
		
	for(Fit f = pw.fleets.begin(); f != pw.fleets.end(); ++f) {
		if(f->DestinationPlanet() != p.PlanetID()) continue; // not relevant

		int dt = f->TurnsRemaining() - s.time;
		s.time = f->TurnsRemaining();
		if(s.owner > 0) s.ships += dt * p.GrowthRate();
		
		if(s.owner == f->Owner())
			s.ships += f->NumShips();
		else {
			s.ships -= f->NumShips();
			if(s.ships < 0) { // change owner
				s.ships *= -1;
				s.owner = f->Owner();
			}
		}
	}

	return s;
}

float averageDistToNotOwnedPlanets(const Planet& p) {
	int num = 0;
	float sum = 0;
	for(Pit i = pw.planets.begin(); i != pw.planets.end(); ++i) {
		if(i->Owner() == 1) continue;
		num++;
		sum += _pw.Distance(p.PlanetID(), i->PlanetID());
	}
	if(num > 0) return sum / float(num);
	return 0.0f;
}

#define SHIPSNEEDEDTOCONQUER 2

float rankPlanet(const Planet& p) {
	RelevantPlanetState s = relevantPlanetState(p);
	Is planets; planets.reserve(_pw.NumPlanets());
	for(int i = 0; i < _pw.NumPlanets(); ++i) {
		if(_pw.GetPlanet(i).Owner() == 1)
			planets.push_back(i);
	}
	sort(planets.begin(), planets.end(), PlanetOrderByDistance(p.PlanetID()));
	
	if(s.owner != 1) {
		int numShips = 0;
		int time = 0;
		for(Iit i = planets.begin(); i != planets.end(); ++i) {
			if(*i == p.PlanetID()) continue; // it is not ours anymore
			int dist = _pw.Distance(*i, p.PlanetID());
			if(dist > time) time = dist;
			numShips += pw.myAvailableShips[*i] - 1; // not all, keep one at least

			if(numShips >= s.ships + SHIPSNEEDEDTOCONQUER) break; // we have enough together
		}
		
		if(numShips < s.ships + SHIPSNEEDEDTOCONQUER) return -1000.0f; // we cannot get it at all right now
		
		return
		10.0f * (1.0f - float(s.ships + SHIPSNEEDEDTOCONQUER) / float(pw.myAvailableShipsNum)) +
		10.0f * expf(-float(time)*0.1f);
	}
	
	// very simple for now -- if we are closer to enemy/neutral, that's better. low priority though
	return
	expf(-averageDistToNotOwnedPlanets(p) * 0.1f);
}

int getBestPlanetByRank() { // only with rank > 0
	int bestP = -1;
	float bestR = 0.0f;
	for(int p = 0; p < _pw.NumPlanets(); ++p) {
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
	for(int i = 0; i < _pw.NumPlanets(); ++i) {
		if(_pw.GetPlanet(i).Owner() == 1)
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
		int dist = _pw.Distance(*i, p);
		pw.fleets.push_back( Fleet(1, sendShipsNum, *i, p, dist, dist) );
		
		pw.myAvailableShips[*i] -= sendShipsNum;
		pw.myAvailableShipsNum -= sendShipsNum;
		neededShips -= sendShipsNum;
		
		if(neededShips <= 0) break;
	}
	
	return true;
}

void DoTurn() {
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
    current_line += (char)c;
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
