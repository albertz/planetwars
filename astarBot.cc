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
	return currentTimeMillis() - startRoundTime >= 600;
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
	int deltaTime;
	Turn() : player(-1), destPlanet(-1), deltaTime(0) {}
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
	double eval() const { return x + y; }
	bool operator<(const NodeScore& c) const { return eval() < c.eval(); }
};

struct Node {
	int time;
	NodeScore scoreSoFar;
	NodeScore score;
	GameState state;
	std::set<Transition> prevNodes;
	std::set<Transition> nextNodes;
	Node() : time(0) {}
	void clear() { prevNodes.clear(); nextNodes.clear(); }
};

int maxForwardTurns = 200;

// this must be an over-estimation, i.e. >= the real value for all possibilities
NodeScore estimateRest(const NodeP& node) {
	NodeScore score;
	int time = node->time;
		
	GameState state = node->state;
	while(state.fleets.size() > 0 && time < maxForwardTurns) {
		score.x += state.Production(1, game.desc);
		score.y += state.Production(2, game.desc);
		state.DoTimeStep(game.desc);
		time++;
	}
	
	if(time < maxForwardTurns) {
		double neutProd = state.Production(0, game.desc); // assume that each player got all the rest
		NodeScore rest;
		rest.x = neutProd + state.Production(1, game.desc);
		rest.y = neutProd + state.Production(2, game.desc);
		rest *= maxForwardTurns - time;
		score += rest;
	}
	
	return score;
}

double p1Score(const NodeP& n) {
	return double(std::max(1, maxForwardTurns - n->time)) * (n->state.Production(1, game.desc) - n->state.Production(2, game.desc));
}

struct Graph {
	typedef Bimap<NodeScore, NodeP> Nodes;
	Nodes nodes; // index/order both by nodes and by cost
	typedef Bimap<double, NodeP> NodesByP1Score;
	NodesByP1Score nodesByP1Score;
	NodeP startNode;
	
	void insert(const NodeP& n) {
		n->score = n->scoreSoFar;
		n->score += estimateRest(n);
		nodes.insert( n->score, n );
		nodesByP1Score.insert( p1Score(n), n );
	}
	void erase(const NodeP& n) {
		nodes.erase2(n);
		nodesByP1Score.erase2(n);
	}
	
	void clear() {
		nodesByP1Score.clear();
		std::list<NodeP> nodes;
		nodes.push_back(startNode);
		while(!nodes.empty()) {
			NodeP node = nodes.front(); nodes.pop_front();
			for(std::set<Transition>::iterator i = node->nextNodes.begin(); i != node->nextNodes.end(); ++i)
				nodes.push_back(i->target);
			node->clear();
		}
	}
	~Graph() { clear(); }
};

void initGraph(Graph& g, const GameState& initialState) {
	NodeP node(new Node);
	node->state = initialState;
	g.startNode = node;
	g.insert(node);
	assert(g.nodes.map1.size() == 1);
	assert(g.nodes.map2.size() == 1);
}

NodeP getHighestScoredNode(Graph& g) {
	assert(g.nodes.map1.size() >= 1);
	Graph::Nodes::T1Map::reverse_iterator i = g.nodes.map1.rbegin();
	assert(i != g.nodes.map1.rend()); // there must at least be one entry
	//cerr << "highest:" << i->first.x << "," << i->first.y << endl;
	return i->second->second();
}

#define SHIPSNEEDEDTOCONQUER 2

void pushbackNode(Turn turn, const NodeP& srcNode, const GameState& nextState, Graph& g) {
	NodeP node(new Node);
	node->time = srcNode->time + turn.deltaTime;
	node->state = nextState;
	
	Transition t(turn, srcNode, node);
	srcNode->nextNodes.insert(t);
	node->prevNodes.insert(t);
	
	node->scoreSoFar = srcNode->scoreSoFar;
	node->scoreSoFar.x += node->state.Production(1, game.desc);
	node->scoreSoFar.y += node->state.Production(2, game.desc);
	g.insert(node);
}

void ExecuteOrder(GameState& state, const GameDesc& desc,
				  int playerID,
				  int sourcePlanet,
				  int destinationPlanet,
				  int numShips) {
	PlanetState& source = state.planets[sourcePlanet];
	assert(source.owner == playerID);
	assert(numShips > 0);
	assert(numShips <= source.numShips);
	
	source.numShips -= numShips;
	int distance = desc.Distance(sourcePlanet, destinationPlanet);
	Fleet f(source.owner,
			numShips,
			sourcePlanet,
			destinationPlanet,
			distance,
			distance);
	state.fleets.push_back(f);
}

static const bool lessFleetBranchingHack = true;
static const bool delayShipSendingIfPossible = true;

static void __pushbackNode(const Turn& turn, const NodeP& srcNode, Fleets::iterator fleetBegin, const Fleets::iterator& fleetEnd, Graph& g) {
	GameState nextState = srcNode->state;
	int num = 0;
	for(; fleetBegin != fleetEnd; ++fleetBegin) {
		const Fleet& f = *fleetBegin;
		if(delayShipSendingIfPossible && f.turnsRemaining != 1) continue;

		ExecuteOrder(nextState, game.desc, turn.player, f.sourcePlanet, turn.destPlanet, f.numShips);
		++num;
	}
	
	if(num > 0)
		pushbackNode(turn, srcNode, nextState, g);
}

void expandNextNodesForPlanet(const Turn& turn, const NodeP& node, Graph& g) {
	Is planets; planets.reserve(game.NumPlanets());
	for(size_t i = 0; i < game.NumPlanets(); ++i) planets.push_back(i);
	sort(planets.begin(), planets.end(), PlanetOrderByDistance(turn.destPlanet));
	
	const GameState& startState = node->state;
	const int planetId = turn.destPlanet;
	
	Fleets fleets = startState.fleets; fleets.reserve(fleets.size() + planets.size());
	const size_t oldFleetsSize = fleets.size();
	PlanetState planet = startState.planets[planetId];
	bool haveMin = (planet.owner == turn.player);

	int numShips = 0;
	int time = 0;
	int minNeededShips = 0;
	int minNeededTime = 0;
	
	for(Iit i = planets.begin(); i != planets.end(); ++i) {
		if(startState.planets[*i].owner != turn.player) continue; // we can only send ships from our own planets
		if(startState.planets[*i].numShips == 0) continue; // we must have ships to send
		int dist = game.desc.Distance(*i, turn.destPlanet);
		while(time + 1 < dist) { // always one before we have arrived
			FleetsTimeStep(fleets);
			planet.DoTimeStep(planetId, game.desc.planets[planetId].growthRate, fleets);
			++time;
		}
		
		Fleet f(turn.player,
				startState.planets[*i].numShips,
				*i,
				planetId,
				1, // we simulate the timeframe right before this fleet is landing
				dist);
		fleets.push_back(f);
		
		PlanetState planetNextFrame = planet.NextTimeStep(planetId, game.desc.planets[planetId].growthRate, fleets, 1); 

		if(!haveMin) {			
			if(planetNextFrame.owner == turn.player) { // got it
				haveMin = true;
				minNeededTime = dist;
				
				// let's see how much we can take away so that we still conquer it
				do {
					minNeededShips = numShips + fleets.back().numShips;

					if(shouldStopRound()) break;
					if(fleets.back().numShips == 1) break;
					fleets.back().numShips--;
				} while(planet.NextTimeStep(planetId, game.desc.planets[planetId].growthRate, fleets, 1).owner == turn.player);

				fleets.back().numShips = startState.planets[*i].numShips;
			}
		}

		if(haveMin) {
			while(true) {
				__pushbackNode(turn, node, fleets.begin() + oldFleetsSize, fleets.end(), g);
				
				if(lessFleetBranchingHack && dist > minNeededTime) break;
				if(shouldStopRound()) break;
				if(fleets.back().numShips == 1) break;
				if(numShips + fleets.back().numShips - 1 < minNeededShips) break;
				fleets.back().numShips--;
			}

			fleets.back().numShips = startState.planets[*i].numShips;
		}
		
		numShips += fleets.back().numShips;
				
		if(shouldStopRound()) break;
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
	if(node->time >= maxForwardTurns) return true;
	
	Turn turn;
	turn.deltaTime = 0;
	turn.player = 1;
	expandNextNodesForPlayer(turn, node, g);
	turn.player = 2;
	expandNextNodesForPlayer(turn, node, g);

	// just do nothing this round
	turn.deltaTime = 1;
	turn.player = -1;
	turn.destPlanet = -1;
	pushbackNode(turn, node, node->state.NextTimeStep(game.desc), g);
	
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
	
/*	for(Graph::NodesByP1Score::T1Map::reverse_iterator i = g.nodesByP1Score.map1.rbegin(); i != g.nodesByP1Score.map1.rend(); ++i) {
		cerr << i->first << ",";
	}
	cerr << endl;
*/	
	assert(g.nodesByP1Score.size() > 0);
	typedef std::list<Transition> Path;
	Path path;
	//NodeP node = getHighestScoredNode(g);
	NodeP node = g.nodesByP1Score.map1.rbegin()->second->second();
	while(node != g.startNode) {
		assert(node->prevNodes.size() > 0);
		const Transition& t = *node->prevNodes.begin();		
		path.push_front(t);
		node = t.source;
	}
	
	GameState wantedState = game.state;
	int dt = 0;
	for(Path::iterator i = path.begin(); i != path.end(); ++i) {
		wantedState = i->target->state;
		dt = i->target->time;
		if(dt > 0) break;
	}
	cerr << "best (" << p1Score(node) << ") path len: " << path.back().target->time << "(" << path.size() << "), nodes: " << g.nodes.size() << ", dt: " << dt << endl;
	
	for(size_t i = 0; i < wantedState.fleets.size(); ++i) {
		const Fleet& fleet = wantedState.fleets[i];
		if(fleet.owner != 1) continue; // dont care
		// turnsRemaining + dt = totalTripLength -> we just have created it
		if(fleet.turnsRemaining + dt != fleet.totalTripLength) continue;
		if(fleet.sourcePlanet == fleet.destinationPlanet) continue; // invalid on server
		if(fleet.numShips == 0) continue; // invalid on server
		
		if(game.state.ExecuteOrder(game.desc, 1, fleet.sourcePlanet, fleet.destinationPlanet, fleet.numShips))
			game.IssueOrder(fleet.sourcePlanet, fleet.destinationPlanet, fleet.numShips);
		else
			cerr << "error for fleet " << fleet.sourcePlanet << "," << fleet.destinationPlanet << "," << fleet.numShips << endl;
	}
	
	game.FinishTurn();
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
				DoTurn();
				map_data = "";
			} else {
				map_data += current_line;
			}
			current_line = "";
		}
	}
	return 0;
}
