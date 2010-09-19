#include <iostream>
#include <algorithm>
#include <vector>
#include <map>
#include <cmath>
#include <string>
#include <set>
#include <tr1/memory>
#include "game.h"
using namespace std;

typedef std::vector<Planet> Ps;
typedef std::vector<Fleet> Fs;
typedef std::vector<int> Is;
typedef Ps::iterator Pit;
typedef Fs::iterator Fit;
typedef Is::iterator Iit;

static Game game;

struct Turn {
	int player;
	int deltaTime;
	int destPlanet;
	int shipsAmount;
};

struct Node;
typedef std::tr1::shared_ptr<Node> NodeP;

struct Transition {
	Turn turn;
	NodeP target, source;
};

struct Node {
	int deltaTime;
	GameState state;
	double costToGetHere;
	std::set<Transition> transitions;
};

struct Graph {
	std::set<NodeP> nodes;
	NodeP startNode;
};

void initGraph(Graph& g, const GameState& initialState) {
	NodeP node = new Node;
	node->deltaTime = 0;
	node->state = initialState;
	node->costToGetHere = 0;
	g.startNode = node;
}

void searchNextNode(Graph& g, int maxForwardTurns) {
	
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
