#ifndef MCTS_HEADER_PETTER
#define MCTS_HEADER_PETTER
//
// Monte Carlo Tree Search for finite games.
//
// Originally based on Python code at
// http://mcts.ai/code/python.html
//
// Uses the "root parallelization" technique [1].
//
namespace minimum {
namespace ai {

// This game engine can play any game defined by a state like this:
/*

class GameState
{
public:
  typedef int Move;
  static const Move no_move = ...

  void do_move(Move move);
  template<typename RandomEngine>
  void do_random_move(*engine);
  bool has_moves() const;
  std::vector<Move> get_moves() const;

  // Returns a value in {0, 0.5, 1}.
  // This should not be an evaluation function, because it will only be
  // called for finished games. Return 0.5 to indicate a draw.
  double get_result(int current_player_to_move) const;

  int player_to_move;

  // ...
private:
  // ...
};

*/
template <typename State>
concept GameState = requires(State state, const State cstate) {
	typename State::Move;
	State::no_move;

	state.do_move(State::no_move);
	cstate.has_moves();
	cstate.get_moves();
	cstate.get_result(0);
	cstate.player_to_move;
};

//
// See the examples for more details. Given a suitable State, the
// following function (tries to) compute the best move for the
// player to move.
//

struct ComputeOptions {
	int number_of_threads;
	int max_iterations;
	double max_time;
	bool verbose;

	ComputeOptions()
	    : number_of_threads(8),
	      max_iterations(10000),
	      max_time(-1.0),  // default is no time limit.
	      verbose(false) {}
};

template <GameState State>
typename State::Move compute_move(const State root_state,
                                  const ComputeOptions options = ComputeOptions());
}  // namespace ai
}  // namespace minimum

//
//
// [1] Chaslot, G. M. B., Winands, M. H., & van Den Herik, H. J. (2008).
//     Parallel monte-carlo tree search. In Computers and Games (pp.
//     60-71). Springer Berlin Heidelberg.
//

#include <algorithm>
#include <cstdlib>
#include <future>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <minimum/core/check.h>
#include <minimum/core/time.h>

namespace minimum {
namespace ai {
using std::cerr;
using std::endl;
using std::size_t;
using std::vector;

//
// This class is used to build the game tree. The root is created by the users and
// the rest of the tree is created by add_node.
//
template <GameState State>
class Node {
   public:
	typedef typename State::Move Move;

	Node(const State& state);
	~Node();

	bool has_untried_moves() const;
	template <typename RandomEngine>
	Move get_untried_move(RandomEngine* engine) const;
	Node* best_child() const;

	bool has_children() const { return !children.empty(); }

	Node* select_child_UCT() const;
	Node* add_child(const Move& move, const State& state);
	void update(double result);

	std::string to_string() const;
	std::string tree_to_string(int max_depth = 1000000, int indent = 0) const;

	const Move move;
	Node* const parent;
	const int player_to_move;

	// std::atomic<double> wins;
	// std::atomic<int> visits;
	double wins;
	int visits;

	std::vector<Move> moves;
	std::vector<Node*> children;

   private:
	Node(const State& state, const Move& move, Node* parent);

	std::string indent_string(int indent) const;

	Node(const Node&);
	Node& operator=(const Node&);

	double UCT_score;
};

/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////

template <GameState State>
Node<State>::Node(const State& state)
    : move(State::no_move),
      parent(nullptr),
      player_to_move(state.player_to_move),
      wins(0),
      visits(0),
      moves(state.get_moves()),
      UCT_score(0) {}

template <GameState State>
Node<State>::Node(const State& state, const Move& move_, Node* parent_)
    : move(move_),
      parent(parent_),
      player_to_move(state.player_to_move),
      wins(0),
      visits(0),
      moves(state.get_moves()),
      UCT_score(0) {}

template <GameState State>
Node<State>::~Node() {
	for (auto child : children) {
		delete child;
	}
}

template <GameState State>
bool Node<State>::has_untried_moves() const {
	return !moves.empty();
}

template <GameState State>
template <typename RandomEngine>
typename State::Move Node<State>::get_untried_move(RandomEngine* engine) const {
	minimum_core_assert(!moves.empty());
	std::uniform_int_distribution<std::size_t> moves_distribution(0, moves.size() - 1);
	return moves[moves_distribution(*engine)];
}

template <GameState State>
Node<State>* Node<State>::best_child() const {
	minimum_core_assert(moves.empty());
	minimum_core_assert(!children.empty());

	return *std::max_element(
	    children.begin(), children.end(), [](Node* a, Node* b) { return a->visits < b->visits; });
	;
}

template <GameState State>
Node<State>* Node<State>::select_child_UCT() const {
	minimum_core_assert(!children.empty());
	for (auto child : children) {
		child->UCT_score = double(child->wins) / double(child->visits)
		                   + std::sqrt(2.0 * std::log(double(this->visits)) / child->visits);
	}

	return *std::max_element(children.begin(), children.end(), [](Node* a, Node* b) {
		return a->UCT_score < b->UCT_score;
	});
}

template <GameState State>
Node<State>* Node<State>::add_child(const Move& move, const State& state) {
	auto node = new Node(state, move, this);
	children.push_back(node);
	minimum_core_assert(!children.empty());

	auto itr = moves.begin();
	for (; itr != moves.end() && *itr != move; ++itr)
		;
	minimum_core_assert(itr != moves.end());
	std::iter_swap(itr, moves.end() - 1);
	moves.pop_back();
	return node;
}

template <GameState State>
void Node<State>::update(double result) {
	visits++;

	wins += result;
	// double my_wins = wins.load();
	// while ( ! wins.compare_exchange_strong(my_wins, my_wins + result));
}

template <GameState State>
std::string Node<State>::to_string() const {
	std::stringstream sout;
	sout << "["
	     << "P" << 3 - player_to_move << " "
	     << "M:" << move << " "
	     << "W/V: " << wins << "/" << visits << " "
	     << "U: " << moves.size() << "]\n";
	return sout.str();
}

template <GameState State>
std::string Node<State>::tree_to_string(int max_depth, int indent) const {
	if (indent >= max_depth) {
		return "";
	}

	std::string s = indent_string(indent) + to_string();
	for (auto child : children) {
		s += child->tree_to_string(max_depth, indent + 1);
	}
	return s;
}

template <GameState State>
std::string Node<State>::indent_string(int indent) const {
	std::string s = "";
	for (int i = 1; i <= indent; ++i) {
		s += "| ";
	}
	return s;
}

/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////

template <GameState State>
std::unique_ptr<Node<State>> compute_tree(const State root_state,
                                          const ComputeOptions options,
                                          std::mt19937_64::result_type initial_seed) {
	std::mt19937_64 random_engine(initial_seed);

	minimum_core_assert(options.max_iterations >= 0 || options.max_time >= 0);
	// Will support more players later.
	minimum_core_assert(root_state.player_to_move == 1 || root_state.player_to_move == 2);
	auto root = std::unique_ptr<Node<State>>(new Node<State>(root_state));

	double start_time = core::wall_time();
	double print_time = start_time;

	for (int iter = 1; iter <= options.max_iterations || options.max_iterations < 0; ++iter) {
		auto node = root.get();
		State state = root_state;

		// Select a path through the tree to a leaf node.
		while (!node->has_untried_moves() && node->has_children()) {
			node = node->select_child_UCT();
			state.do_move(node->move);
		}

		// If we are not already at the final state, expand the
		// tree with a new node and move there.
		if (node->has_untried_moves()) {
			auto move = node->get_untried_move(&random_engine);
			state.do_move(move);
			node = node->add_child(move, state);
		}

		// We now play randomly until the game ends.
		while (state.has_moves()) {
			state.do_random_move(&random_engine);
		}

		// We have now reached a final state. Backpropagate the result
		// up the tree to the root node.
		while (node != nullptr) {
			node->update(state.get_result(node->player_to_move));
			node = node->parent;
		}

		if (options.verbose || options.max_time >= 0) {
			double time = core::wall_time();
			if (options.verbose && (time - print_time >= 1.0 || iter == options.max_iterations)) {
				std::cerr << iter << " games played (" << double(iter) / (time - start_time)
				          << " / second)." << endl;
				print_time = time;
			}

			if (time - start_time >= options.max_time) {
				break;
			}
		}
	}

	return root;
}

template <GameState State>
typename State::Move compute_move(const State root_state, const ComputeOptions options) {
	using namespace std;

	// Will support more players later.
	minimum_core_assert(root_state.player_to_move == 1 || root_state.player_to_move == 2);

	auto moves = root_state.get_moves();
	minimum_core_assert(moves.size() > 0);
	if (moves.size() == 1) {
		return moves[0];
	}

	double start_time = core::wall_time();

	// Start all jobs to compute trees.
	vector<future<unique_ptr<Node<State>>>> root_futures;
	ComputeOptions job_options = options;
	job_options.verbose = false;
	for (int t = 0; t < options.number_of_threads; ++t) {
		auto func = [t, &root_state, &job_options]() -> std::unique_ptr<Node<State>> {
			return compute_tree(root_state, job_options, 1012411 * t + 12515);
		};

		root_futures.push_back(std::async(std::launch::async, func));
	}

	// Collect the results.
	vector<unique_ptr<Node<State>>> roots;
	for (int t = 0; t < options.number_of_threads; ++t) {
		roots.push_back(std::move(root_futures[t].get()));
	}

	// Merge the children of all root nodes.
	map<typename State::Move, int> visits;
	map<typename State::Move, double> wins;
	long long games_played = 0;
	for (int t = 0; t < options.number_of_threads; ++t) {
		auto root = roots[t].get();
		games_played += root->visits;
		for (auto child = root->children.cbegin(); child != root->children.cend(); ++child) {
			visits[(*child)->move] += (*child)->visits;
			wins[(*child)->move] += (*child)->wins;
		}
	}

	// Find the node with the most visits.
	double best_score = -1;
	typename State::Move best_move = typename State::Move();
	for (auto itr : visits) {
		auto move = itr.first;
		double v = itr.second;
		double w = wins[move];
		// Expected success rate assuming a uniform prior (Beta(1, 1)).
		// https://en.wikipedia.org/wiki/Beta_distribution
		double expected_success_rate = (w + 1) / (v + 2);
		if (expected_success_rate > best_score) {
			best_move = move;
			best_score = expected_success_rate;
		}

		if (options.verbose) {
			cerr << "Move: " << itr.first << " (" << setw(2) << right
			     << int(100.0 * v / double(games_played) + 0.5) << "% visits)"
			     << " (" << setw(2) << right << int(100.0 * w / v + 0.5) << "% wins)" << endl;
		}
	}

	if (options.verbose) {
		auto best_wins = wins[best_move];
		auto best_visits = visits[best_move];
		cerr << "----" << endl;
		cerr << "Best: " << best_move << " (" << 100.0 * best_visits / double(games_played)
		     << "% visits)"
		     << " (" << 100.0 * best_wins / best_visits << "% wins)" << endl;
	}

	if (options.verbose) {
		double time = core::wall_time();
		std::cerr << games_played << " games played in " << double(time - start_time) << " s. "
		          << "(" << double(games_played) / (time - start_time) << " / second, "
		          << options.number_of_threads << " parallel jobs)." << endl;
	}

	return best_move;
}
}  // namespace ai
}  // namespace minimum

#endif
