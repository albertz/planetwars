#include <iostream>
#include <algorithm>
#include <vector>
#include <map>
#include <cmath>
#include <string>
#include <set>
#include <list>
#include <tr1/memory>
#include <cassert>
#include "SimpleBimap.h"
#include "game.h"
#include "vec.h"
#include "utils.h"

using namespace std;

typedef std::vector<Planet> Ps;
typedef std::vector<Fleet> Fs;
typedef std::vector<int> Is;
typedef Ps::iterator Pit;
typedef Fs::iterator Fit;
typedef Is::iterator Iit;

static Game game;
static long startRoundTime;

static bool shouldStopRound() {
	return currentTimeMillis() - startRoundTime >= 10000;
}

struct PlanetOrderByDistance {
	VecD pos;
	PlanetOrderByDistance(int planet) : pos(game.desc.planets[planet].pos()) {}
	PlanetOrderByDistance(VecD _pos) : pos(_pos) {}
	
	bool operator()(int p1, int p2) const {
		return (game.desc.planets[p1].pos() - pos).GetLength2() < (game.desc.planets[p2].pos() - pos).GetLength2();
	}
};




struct Turn {
	int player;
	int destPlanet;
	int shipsAmount;
	int deltaTime;
	Turn() : player(-1), destPlanet(-1), shipsAmount(0), deltaTime(0) {}
};

struct Node;
typedef std::tr1::shared_ptr<Node> NodeP;

struct Transition {
	Turn turn;
	NodeP source, target;

	Transition(Turn _turn, const NodeP& _source, const NodeP& _target)
	: turn(_turn), source(_source), target(_target) {}
	bool operator<(const Transition& t) const {
		if(target.get() != t.target.get()) return target.get() < t.target.get();
		return source.get() < t.source.get();
	}
};

struct NodeScore : VecD { // must be >= 0
//	double eval() const { return (x + y) / (1.0 + abs(x - y)); }
	double eval() const { return x - y; }
	bool operator<(const NodeScore& c) const { return eval() < c.eval(); }
};

struct Node {
	Vec time; // per player
	NodeScore scoreSoFar;
	NodeScore score;
	GameState state;
	std::set<Transition> prevNodes;
	std::set<Transition> nextNodes;
	void clear() { prevNodes.clear(); nextNodes.clear(); }
};

struct Graph {
	typedef Bimap<NodeScore, NodeP> Nodes;
	Nodes nodes; // index/order both by nodes and by cost
	NodeP startNode;
	
	Nodes::EntryP insert(const NodeP& n) { return nodes.insert( n->score, n ); }
	void erase(const NodeP& n) { nodes.erase2(n); }
	~Graph() {
		startNode.reset();
		for(Nodes::T1Iter i = nodes.map1.begin(); i != nodes.map1.end(); ++i)
			NodeP(i->second->second())->clear();
	}
};

int maxForwardTurns = 200;

// this must be an over-estimation, i.e. >= the real value for all possibilities
NodeScore estimateRest(const NodeP& node) {
	NodeScore score;

	GameState state = node->state;
	int time = std::min(node->time.x, node->time.y);
	while(state.fleets.size() > 0 && time < maxForwardTurns) {
		score.x += state.Production(1, game.desc);
		score.y += state.Production(2, game.desc);
		state.DoTimeStep(game.desc);
		time++;
	}
	
	if(time < maxForwardTurns) {
		double neutProd = 0; //state.Production(0, game.desc); // assume that each player got all the rest
		score.x += neutProd + state.Production(1, game.desc);
		score.y += neutProd + state.Production(2, game.desc);
		score *= maxForwardTurns - time;
	}
	return score;
}

void initGraph(Graph& g, const GameState& initialState) {
	NodeP node(new Node);
	node->state = initialState;
	node->score = estimateRest(node);
	g.startNode = (NodeP)g.insert(node)->second();
	assert(g.nodes.map1.size() == 1);
	assert(g.nodes.map2.size() == 1);
}

NodeP getHighestScoredNode(Graph& g) {
	assert(g.nodes.map1.size() >= 1);
	Graph::Nodes::T1Map::reverse_iterator i = g.nodes.map1.rbegin();
	assert(i != g.nodes.map1.rend()); // there must at least be one entry
	return i->second->second();
}

#define SHIPSNEEDEDTOCONQUER 2

/*
// not perfect yet
// the first state where we got it
// maybe the last state is better? but we will explore it anyway
bool shipsNeededToConquerMin(int& numShips, int& time, int planet, int playerId, const GameState& startState, GameState& stateWithFleets) {
	Is planets; planets.reserve(game.NumPlanets());
	for(size_t i = 0; i < game.NumPlanets(); ++i) planets.push_back(i);
	sort(planets.begin(), planets.end(), PlanetOrderByDistance(planet));

	stateWithFleets = startState;
	numShips = 0;
	time = 0;
	for(Iit i = planets.begin(); i != planets.end(); ++i) {
		if(*i == planet) continue; // that is the planet we want to conquer
		if(startState.planets[*i].owner != playerId) continue; // we can only send ships from our own planets
		int dist = game.desc.Distance(*i, planet);
		if(dist > time) time = dist;
		stateWithFleets.ExecuteOrder(game.desc, playerId, *i, planet, startState.planets[*i].numShips);
		
		GameState state = stateWithFleets;
		state.DoTimeSteps(dist, game.desc);
		if(state.planets[planet].owner == playerId) { // got it
			// let's see how much we can take away so that we still conquer it
			while(state.planets[planet].owner == playerId) {
				stateWithFleets.fleets.back().numShips--;
				state = stateWithFleets;
				state.DoTimeSteps(dist, game.desc);
			}
			stateWithFleets.fleets.back().numShips++;
			numShips += stateWithFleets.fleets.back().numShips;
			return true;
		}
		
		numShips += stateWithFleets.fleets.back().numShips;
	}	
	// no chance
	return false;
}

// not perfect and even wrong. also not used atm
// max would be when enemy is sending each new round also
void shipsNeededToConquerMax(int& numShips, int& time, int planet, int playerId, GameState startState) {
	// add maximum possible fleets from enemy
	for(size_t i = 0; i < game.NumPlanets(); ++i) {
		if(i == planet) continue;
		if(startState.planets[i].owner == 0) continue;
		if(startState.planets[i].owner == playerId) continue;
		startState.ExecuteOrder(game.desc, startState.planets[i].owner,
								i, planet, startState.planets[i].numShips);
	}
	shipsNeededToConquerMin(numShips, time, planet, playerId, startState);
}*/

void pushbackNode(Turn turn, const NodeP& srcNode, const GameState& nextState, Graph& g) {
	NodeP node(new Node);
	node->time = srcNode->time;
	((turn.player == 1) ? node->time.x : node->time.y) += turn.deltaTime;
	node->state = nextState;
	
	Transition t(turn, srcNode, node);
	srcNode->nextNodes.insert(t);
	node->prevNodes.insert(t);
	
	node->scoreSoFar = srcNode->scoreSoFar;
	node->scoreSoFar.x += node->state.Production(1, game.desc);
	node->scoreSoFar.y += node->state.Production(2, game.desc);
	node->score = node->scoreSoFar;
	node->score += estimateRest(node);
	g.insert(node);
}

void expandNextNodesForDT(Turn turn, const NodeP& srcNode, const GameState& nextState, Graph& g) {
	turn.deltaTime = 0;
	pushbackNode(turn, srcNode, nextState, g);
	turn.deltaTime = 1;
	pushbackNode(turn, srcNode, nextState.NextTimeStep(game.desc), g);
}

void expandNextNodesForPlanet(Turn turn, const NodeP& node, Graph& g) {
	Is planets; planets.reserve(game.NumPlanets());
	for(size_t i = 0; i < game.NumPlanets(); ++i) planets.push_back(i);
	sort(planets.begin(), planets.end(), PlanetOrderByDistance(turn.destPlanet));
	
	const GameState& startState = node->state;
	bool haveMin = startState.planets[turn.destPlanet].owner == turn.player;
	GameState stateWithFleets = startState;
	int numShips = 0;
	int time = 0;
	for(Iit i = planets.begin(); i != planets.end(); ++i) {
		if(*i == turn.destPlanet) continue; // that is the planet we want to conquer
		if(startState.planets[*i].owner != turn.player) continue; // we can only send ships from our own planets
		if(startState.planets[*i].numShips == 0) continue; // we must have ships to send
		int dist = game.desc.Distance(*i, turn.destPlanet);
		if(dist > time) time = dist;
				
		if(!haveMin) {
			assert(stateWithFleets.ExecuteOrder(game.desc, turn.player, *i, turn.destPlanet, startState.planets[*i].numShips));

			GameState state = stateWithFleets;
			state.DoTimeSteps(dist, game.desc);
			if(state.planets[turn.destPlanet].owner == turn.player) { // got it
				// let's see how much we can take away so that we still conquer it
				while(state.planets[turn.destPlanet].owner == turn.player) {
					turn.shipsAmount = numShips + stateWithFleets.fleets.back().numShips;
					expandNextNodesForDT(turn, node, stateWithFleets, g);

					if(shouldStopRound()) return;
					if(stateWithFleets.fleets.back().numShips == 1) break;
					stateWithFleets.fleets.back().numShips--;
					stateWithFleets.planets[*i].numShips++;
					state = stateWithFleets;
					state.DoTimeSteps(dist, game.desc);
				}
				
				stateWithFleets.fleets.back().numShips = startState.planets[*i].numShips;
				stateWithFleets.planets[*i].numShips = 0;
				haveMin = true;
			}
		}
		else {
			assert(stateWithFleets.ExecuteOrder(game.desc, turn.player, *i, turn.destPlanet, 1));
			while(true) {
				turn.shipsAmount = numShips + stateWithFleets.fleets.back().numShips;
				expandNextNodesForDT(turn, node, stateWithFleets, g);
				
				if(shouldStopRound()) return;
				if(stateWithFleets.planets[*i].numShips == 0) break;
				stateWithFleets.fleets.back().numShips++;
				stateWithFleets.planets[*i].numShips--;
			}
		}
		
		numShips += stateWithFleets.fleets.back().numShips;
		
		if(shouldStopRound()) return;
	}
}

static VecD averagePosOfPlayer(const GameState& state, int player) {
	double numShips = 0;
	VecD pos;
	for(size_t i = 0; i < state.planets.size(); ++i) {
		if(state.planets[i].owner != player) continue;
		numShips += state.planets[i].numShips;
		pos
		+= VecD(game.desc.planets[i].x, game.desc.planets[i].y)
		* state.planets[i].numShips;
	}
	if(numShips > 0.001)
		pos *= 1.0 / numShips;
	return pos;
}

void expandNextNodesForPlayer(Turn turn, const NodeP& node, Graph& g) {
	// this pre-sorting is only needed if we break the calculation in the middle and want the best first
	/*
	Is planets; planets.reserve(game.NumPlanets());
	for(size_t i = 0; i < game.NumPlanets(); ++i) planets.push_back(i);
	sort(planets.begin(), planets.end(), PlanetOrderByDistance(averagePosOfPlayer(node->state, turn.player)));
	 */
	
	for(size_t i = 0; i < game.NumPlanets(); ++i) {
		turn.destPlanet = i; //planets[i];
		expandNextNodesForPlanet(turn, node, g);
	}
}

bool expandNextNodes(Graph& g) {
	assert(g.nodes.map1.size() > 0);
	NodeP node = getHighestScoredNode(g);
	if(std::min(node->time.x,node->time.y) >= maxForwardTurns) return true;
	
	Turn turn;
	if(node->time.x <= node->time.y)
		turn.player = 1;
	else
		turn.player = 2;
	expandNextNodesForPlayer(turn, node, g);	

	if(shouldStopRound()) return false; // if we have stopped calc in the middle, just keep
	g.erase(node); // remove this node from search set. will still be in transition paths
	return false;
}



void DoTurn() {
	startRoundTime = currentTimeMillis();
	
	Graph g;
	initGraph(g, game.state);
	
	while(!shouldStopRound()) {
		if(expandNextNodes(g)) break; // we got the maximum len if it returns true
		if(g.nodes.size() == 0) return; // error. can happen at very end
	}
	
	typedef std::list<Transition> Path;
	Path path;
	NodeP node = getHighestScoredNode(g);
	while(node != g.startNode) {
		assert(node->prevNodes.size() > 0);
		const Transition& t = *node->prevNodes.begin();		
		path.push_front(t);
		node = t.source;
	}
	cerr << "best path len: " << path.size() << ", nodes: " << g.nodes.size() << endl;
	
	GameState wantedState = game.state;
	int dt = 0;
	for(Path::iterator i = path.begin(); i != path.end(); ++i) {
		wantedState = i->target->state;
		dt = i->turn.deltaTime;
		if(dt > 0) break;
	}
	
	for(size_t i = 0; i < wantedState.fleets.size(); ++i) {
		const Fleet& fleet = wantedState.fleets[i];
		if(fleet.owner != 1) continue; // dont care
		// turnsRemaining + dt = totalTripLength -> we just have created it
		if(fleet.turnsRemaining + dt != fleet.totalTripLength) continue;

		game.IssueOrder(fleet.sourcePlanet, fleet.destinationPlanet, fleet.numShips);
	}
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
				game.ParseGameState(map_data);
				game.numTurns++;
				map_data = "";
				DoTurn();
				game.FinishTurn();
			} else {
				map_data += current_line;
			}
			current_line = "";
		}
	}
	return 0;
}
