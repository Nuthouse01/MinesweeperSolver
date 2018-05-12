/* 
MinesweeperProject.cpp
Brian Henson, 4/2/2018, v4.7
This whole program (and all solver logic within it) was developed all on my own, without looking up anything
online. I'm sure someone else has done their PHD thesis on creating the 'perfect minesweeper solver' but I
didn't look at any of their work, I developed these strategies all on my own.

Contents: 
MinesweeperProject.cpp, stdafx.cpp, stdafx.h, verhist.txt
*/

// NOTE: v4.4, moved version history to verhist.txt because VisualStudio is surprisingly awful at scanning long block-comments




// TODO: could rearchitect the multicell loop so that 121-cross gets more use... is that useful? is that efficient?
// TODO: test algorithm speed/accuracy with choosing a pod in the middle, or in the front
// TODO: change the logfile print method to instead use >> so if it unexpectedly dies I still have a partial logfile, and/or can read it
//		while the code is running? very anoying how 'release' mode optimizes away the fflush command
// TODO: split code between multiple files


// NOTE: could turn the field object into a struct, and turn many functions/variables into members... lots of effort for no real benefit
// other than object-based code



// from targetver.h:
#include <SDKDDKVer.h> // not sure what this is but Visual Studio wants me to have it
 
#include "stdafx.h" // Visual Studio requires this, even if it adds nothing


#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string>
#include <cstring>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <list>
#include <cassert> // so it aborts when something wierd happens
#include <time.h>

#include <chrono> // just because time(0) only has 1-second resolution
#include <cstdarg> // for variable-arg function-macro
#include <algorithm> // for pod-based intelligent recursion
#include <random> // because I don't like using sequential seeds, or waiting when going very fast


#define _USE_MATH_DEFINES
#include <math.h>





// the various cell states
enum cell_state : short {
	UNKNOWN,			// cell contents are not known
	VISIBLE,			// cell has been revealed; hopefully contains an adjacency number
	SATISFIED,			// all 8 adjacent cells are either flagged or visible; no need to think about this one any more
	FLAGGED				// logically determined to be a mine (but it don't know for sure)
};

#define MINE 50

// game status variable: BEGINNING/SINGLECELL/MULTICELL(further break into the various strategies?)/HUNTING/GUESSING
enum game_state : short {
	SINGLECELL,
	MULTICELL,
	HUNTING,
	GUESSING
};


// one square of the minesweeper grid
struct cell {
	cell() {} // empty constructor
	cell(int xa, int ya) {
		x = xa;
		y = ya;
		status = UNKNOWN;
		value = 0;
		effective = 0;
	}
	int x, y;
	cell_state status;
	short int value;
	short int effective;
};

// stats for a single game
struct game_stats {
	game_stats() {
		trans_map = std::string();
		strat_121 = 0;
		strat_nov_safe = 0;
		strat_nov_flag = 0;
		times_guessing = 0;
		gamestate = GUESSING; // first stage is a blind guess
		began_solving = false;
	}
	int strat_121, strat_nov_safe, strat_nov_flag; // number of times each MC strategy was used
	int times_guessing; // number of times it needed to do hunting or guessing
	//records the phase transition history of solving the game, and the # of operations done in each phase
	// ^#=hunting, G#=guessing, s#=single-cell, m#=multi-cell, A=advanced
	std::string trans_map;
	game_state gamestate; // where in the solver algorithm I am, to track where it was when it ended
	bool began_solving; // did I begin solving things, or did I lose before anything could be done?
};

// win/loss stats for a single program run
struct run_stats {
	run_stats() {
		std::chrono::milliseconds ms = std::chrono::duration_cast< std::chrono::milliseconds >(
			std::chrono::system_clock::now().time_since_epoch()
			);
		start = ms.count();
		games_total = 0;
		games_won = 0;
		games_won_noguessing = 0;
		games_won_guessing = 0;
		games_lost = 0;
		games_lost_beginning = 0;
		games_lost_unexpectedly = 0;
		games_lost_lategame = 0;
		games_lost_midgame = 0;
		games_lost_earlygame = 0;
		strat_121_total = 0;
		strat_nov_safe_total = 0;
		strat_nov_flag_total = 0;
		smartguess_attempts = 0;
		smartguess_diff = 0.;
		smartguess_valves_triggered = 0;
		games_with_eights = 0;
		num_guesses_total = 0;
	}

	void print_final_stats();
	long start;
	int games_total;			// total games played to conclusion (not really needed but w/e)

	int games_won;				// total games won
	int games_won_noguessing;	// games won without "end-game guessing"
	int games_won_guessing;		// games won by "end-game guessing"

	int games_lost;				// total games lost
	int games_lost_beginning;	// games lost before any logic could be applied (0 when hunting correctly)
	int games_lost_earlygame;	// 0-15% completed
	int games_lost_midgame;		// 15-85% completed
	int games_lost_lategame;	// 85-99% completed
	int games_lost_unexpectedly;// losses from other situations (should be 0)

	int strat_121_total;
	int strat_nov_safe_total;
	int strat_nov_flag_total;
	int num_guesses_total;

	int smartguess_attempts;
	float smartguess_diff;
	int smartguess_valves_triggered;
	int games_with_eights;
};



///////////////////////////////////////////////////////////////////////////////////////////////
// struct declarations without the actual contents


///////////////////////////////////////////////////////////////////////////////////////////////
// new pod-based architecture, HEAVILY object-based unlike how it was before
// tons of thinking and writing but it may just be completely scrapped depending on how it performs :(



struct solutionobj {
	solutionobj() {}
	solutionobj(float ans, int howmany) {
		answer = ans; allocs = howmany;
		solution_flag = std::list<struct cell *>(); // init as empty list
	}
	float answer; // the number of mines determined to be in the chain 
	int allocs; // how many allocations this answer corresponds to
	std::list<struct cell *> solution_flag; // cells to flag to apply this solution
};


// holds the list of all allocations found, as a struct for some extra utility
// to combine the results of recursion on separate chains, average the results from each chain and sum them
struct podwise_return {
	podwise_return() { // construct empty
		solutions = std::list<struct solutionobj>();
		effort = 0;
	}
	podwise_return(float ans, int howmany) { // construct from one value
		solutions = std::list<struct solutionobj>(1, solutionobj(ans, howmany));
		effort = 1;
	}

	// podwise_return += list of cell pointers: append into all solutions
	inline struct podwise_return& operator+=(const std::list<struct cell *>& addlist) {
		for (std::list<struct solutionobj>::iterator listit = solutions.begin(); listit != solutions.end(); listit++) {
			listit->solution_flag.insert(listit->solution_flag.end(), addlist.begin(), addlist.end());
		}
		return *this;
	}
	// podwise_return *= int: multiply into all 'allocs' in the list
	inline struct podwise_return& operator*=(const int& rhs) {
		for (std::list<struct solutionobj>::iterator listit = solutions.begin(); listit != solutions.end(); listit++) {
			listit->allocs *= rhs;
		}
		return *this;
	}
	// podwise_return += int: increment all 'answers' in the list
	inline struct podwise_return& operator+=(const int& rhs) {
		for (std::list<struct solutionobj>::iterator listit = solutions.begin(); listit != solutions.end(); listit++) {
			listit->answer += rhs;
		}
		return *this;
	}
	// podwise_return += podwise_return: combine
	inline struct podwise_return& operator+=(const struct podwise_return& rhs) {
		solutions.insert(solutions.end(), rhs.solutions.begin(), rhs.solutions.end());
		effort += rhs.effort;
		return *this;
	}
	
	inline int size() { return solutions.size(); }
	// returns value 0-1 showing what % of allocations were NOT tried (because they were found redundant)
	inline float efficiency() {
		float t1 = float(effort) / float(total_alloc());
		float t2 = 1. - t1;
		if (t2 > 0.) { return t2; } else { return 0.; }
	}
	//inline void validate();
	float avg();
	struct solutionobj * max();
	float max_val();
	struct solutionobj * min();
	float min_val();
	int total_alloc();

	std::list<struct solutionobj> solutions; // list of all allocations found; unsorted, includes many duplicates
	// represents # of solutions found, perseveres even after .avg function
	int effort;
};




// represents the adjacent unknown cells of a visible adjacency number
// almost certain to have some overlap with other pods; these are recorded as 'links'
struct pod {
	pod() {}
	pod(struct cell * new_root);
	// shortcut to calculate the risk for a pod
	float risk() { return (100. * float(mines) / float(size())); }
	// if outside recursion, return cell_list.size(); if inside recursion, return cell_list_size
	int size() { if (cell_list_size < 0) { return cell_list.size(); } else { return cell_list_size; } }
	// scan the pod for any "special cases", perform switch() on result:
	// 0=normal, 1=negative risk, 2=0 risk, 3=100 risk, 4=disjoint, 
	int scan() {
		if (links.empty()) { return 4; }
		float z = risk(); // might crash if it tries to divide by size 0, so disjoint-check must be first
		if (z < 0.) { return 1; }
		if (z == 0.) { return 2; }
		if (z == 100.) { return 3; }
		return 0;
	}
	void add_link(struct cell * shared, struct cell * shared_root);
	int remove_link(struct cell * shared, bool isaflag);
	std::list<struct scenario> pod::find_scenarios();
	std::list<struct link>::iterator get_link(int l);
	struct cell * root; //the visible adjacency-cell the pod is based on (or one of them if there were dupes)
	int mines;			//how many mines are in the pod
	std::list<struct link> links; //cells shared by other pods... list because will often remove from this
	std::vector<struct cell *> cell_list;  //all the cells in the pod, including link cells... NOTE: only used OUTSIDE recusion!
	int cell_list_size; // number of cells in the pod, including link cells... NOTE: only used INSIDE recursion!
	int chain_idx;		// used to identify chains... NOTE: only used OUTSIDE recursion!
};

// represents an overlap cell shared by 2 or more pods
struct link {
	link() {}
	link(struct cell * shared, struct cell * shared_root);
	inline bool operator== (const struct link o) const; // need this to use the 'remove by value' function
	struct cell * link_cell;
	std::list<struct cell *> linked_roots;
};

// chain = a group of pods. when recursing, a chain begins as a group of interconnected/overlapping pods...
// when not recursing its probably the "master chain" which isn't a chain at all, just the set of all pods
struct chain {
	chain();
	std::list<struct pod>::iterator root_to_pod(struct cell * linked_root);
	std::list<struct pod>::iterator int_to_pod(int f);
	int identify_chains();
	void identify_chains_recurse(int idx, struct pod * me);
	std::vector<struct chain> sort_into_chains(int r, bool reduce);
	std::list<struct pod> podlist; // the only member, a list of (probably) overlapping pods
};

// only returned by riskholder calculation, could use a tuple instead but i dont wanna learn another new thing
struct riskreturn {
	riskreturn() {};
	riskreturn(float m, std::list<struct cell *> * l) {
		minrisk = m;
		minlist = *l;
	}
	float minrisk;
	std::list<struct cell *> minlist;
};
// only used to tie together links to remove with a boolean; could use a tuple instead but i dont wanna
struct link_with_bool {
	link_with_bool() {}
	link_with_bool(struct link asdf, bool f) {
		flagme = f;
		l = asdf;
	}
	bool flagme;
	struct link l;
};

// an immitation of the 'field' object, to be allocated statically (globally??) and reused each time the pod-smart-guess is used
// holds the risk info for each cell, will calculate the border cells with the lowest risk
struct riskholder {
	riskholder() {}
	riskholder(int x, int y);

	// takes a cell pointer and adds a risk to its list
	// could modify to use x/y, but why bother?
	void addrisk(struct cell * foo, float newrisk) {
		(riskarray[foo->x][foo->y]).riskvect.push_back(newrisk);
	}
	// find the avg of the risks and return that average; also clear the list. if list is already empty, return -1
	// PRIVATE! internal use only
	float finalrisk(int x, int y) {
		if ((riskarray[x][y]).riskvect.empty())
			return -1.;
		float sum = 0; int s = (riskarray[x][y]).riskvect.size();
		for (int i = 0; i < s; i++) {
			sum += (riskarray[x][y]).riskvect[i];
		}
		(riskarray[x][y]).riskvect.clear(); // clear it for use next time
		return (sum / float(s));
	}
	struct riskreturn findminrisk();

	// a struct used only within this struct
	struct riskcell {
		riskcell() {
			riskvect = std::vector<float>(); // why not
			riskvect.reserve(8); // maximum size of vector is 8 (but highly unlikely)
		}
		std::vector<float> riskvect; // list of risks from various sources
	};
	// immitation of the 'field' for holding risk info, can be quickly accessed with x and y
	std::vector<std::vector<struct riskcell>> riskarray;
};

// bundles the list of links to flag with an allocation #
struct scenario {
	scenario() {}
	scenario(std::list<std::list<struct link>::iterator> map, int num) {
		roadmap = map;
		allocs = num;
	}
	std::list<std::list<struct link>::iterator> roadmap; // list of link-list-iterators to flag
	int allocs; // how many possible allocations this scenario stands in for
};

// returned by smartguess function
struct smartguess_return {
	smartguess_return() {}
	int method; // 0=optimization, 1=guess, 2=solve
	bool empty() { return clearme.empty() && flagme.empty(); }
	int size() { return clearme.size() + flagme.size(); }
	std::vector<struct cell *> clearme;
	std::vector<struct cell *> flagme;
};






// just because I'm sick of calling printf_s and fprintf_s with the same args
// function-like macro that accepts a variable number/type of args, very nifty
// always prints to the logfile; printing to screen depends on #SCREEN_var
// 0=noprint, 1=log only, 2=both
#define myprintfn(p, fmt, ...) { \
	fprintf_s(logfile, fmt, __VA_ARGS__); \
	if(p >= 2) {printf_s(fmt, __VA_ARGS__);} \
}






/*
example distributions:
X/Y/mines
9/9/10
16/16/40
30/16/99
10/18/27
10/18/38
*/

// #defines
// sets the "default values" for each setting
#define NUM_GAMES_def				10000
#define SIZEX_def					30
#define SIZEY_def					16
#define NUM_MINES_def				90
#define	FIND_EARLY_ZEROS_def		true
#define RANDOM_USE_SMART_def		false
#define VERSION_STRING_def			"v4.8"
// controls what gets printed to the console
// 0: prints almost nothing to screen, 1: prints game-end to screen, 2: prints everything to screen
#define SCREEN_def					0
#define ACTUAL_DEBUG				0
// if SPECIFY_SEED_var = 0, will generate a new seed from current time
#define SPECIFY_SEED_def			0


// after X loops, see if single-cell logic can take over... if not, will resume multicell
// surprisingly multicell logic seems to consume even more time than the recursive smartguess when this value is high
#define MULTICELL_LOOP_CUTOFF		3
// in recursive function, after recursing X layers down, check to see if the chain is fragmented (almost certainly is)
// this will have no impact on the accuracy of the result, and testing has shown that over many games it has almost no impact on efficiency
// NOTE: this must be >= RETURN_ACTUAL_ALLOCATION_IF_THIS_MANY_MINES_REMAIN
#define CHAIN_RECHECK_DEPTH			8
// when there are fewer than X mines on the field, begin storing the actual cells flagged to create each answer found by recursion
// also attempt to solve the puzzle if there is exactly one perfect solution
// NOTE: this must be <= CHAIN_RECHECK_DEPTH
#define RETURN_ACTUAL_ALLOCATION_IF_THIS_MANY_MINES_REMAIN	8
// in recursive function, after finding X solutions, stop being so thorough... only test RECURSION_SAFE_WIDTH scenarios at each lvl
// this comes into play only very rarely even when set as high as 10k
// with the limiter at 10k, the highest # of solutions found  is 51k, even then the algorithm takes < 1s
// without the limiter, very rare 'recursion rabbitholes' would find as many as 4.6million solutions to one chain, with very big chains
#define RECURSION_SAFETY_LIMITER	10000 
// when the 'safety valve' is tripped, try X scenarios at each level of recursion, no more
// probably should only be either 2 or 3
#define RECURSION_SAFE_WIDTH		2 
// in recursion, when taking the average of all solutions, weigh them by their relative likelihood or just do an unweighted avg
// this SHOULD decrease algorithm deviation and therefore increase winrate, but instead it somehow INCREASES average deviation and winrate is unchanged
// doesn't make sense at all >:(
#define USE_WEIGHTED_AVG			true


// global vars
int NUM_GAMES_var = 0;
int SIZEX_var = 0;
int SIZEY_var = 0;
int NUM_MINES_var = 0;
bool FIND_EARLY_ZEROS_var = false;
bool RANDOM_USE_SMART_var = false;
int SPECIFY_SEED_var = 0;
int SCREEN_var = 0;

int mines_remaining = 0;
std::list<struct cell *> zerolist; // zero-list
std::list<struct cell *> unklist; // unknown-list, hopefully allows for faster "better rand"
std::vector<std::vector<struct cell>> field; // the actual playing field; overwritten for each game
FILE * logfile; // file stream to the logfile (globally accessible)
struct game_stats mygamestats; // stat-tracking variables
struct run_stats myrunstats; // stat-tracking variables
struct riskholder myriskholder; // for pod-wise smart guessing
bool recursion_safety_valve = false; // if recursion goes down a rabbithole, start taking shortcuts



/*
NOTE: even with "random" hunting, when there are no more zeroes left, it counts as "end-game guessing"
*/


// function declarations
// big/general/fundamental actions that deserve to be functions

inline struct cell * cellptr(int x, int y);														// could be a member function
std::vector<struct cell *> get_adjacent(struct cell * me);										// could be a member function?
int reveal(struct cell * me);																	// could be a member function
int set_flag(struct cell * me);																	// could be a member function
std::vector<struct cell *> filter_adjacent(struct cell * me, cell_state target);				// could be a member function
std::vector<struct cell *> filter_adjacent(std::vector<struct cell *> adj, cell_state target);	// could be a member function
std::vector<std::vector<struct cell *>> find_nonoverlap(std::vector<struct cell *> me_unk, std::vector<struct cell *> other_unk); // general utility
std::list<std::vector<int>> comb(int K, int N); // general utility
inline int comb_int(int K, int N); // general utility
inline int factorial(int x); // general utility
struct cell * rand_from_list(std::list<struct cell *> * fromme); // general utility

// functions for the recursive smartguess method(s)

struct smartguess_return smartguess();
struct podwise_return podwise_recurse(int rescan_counter, struct chain mychain);

// small but essential utility functions

inline bool sort_by_position(struct cell * a, struct cell * b);
inline int compare_two_cells(struct cell * a, struct cell * b);
inline int compare_list_of_cells(std::list<struct cell *> a, std::list<struct cell *> b);
bool sort_scenario_blind(std::list<struct link>::iterator a, std::list<struct link>::iterator b);
inline int compare_list_of_scenarios(std::list<std::list<struct link>::iterator> a, std::list<std::list<struct link>::iterator> b);
bool sort_list_of_scenarios(std::list<std::list<struct link>::iterator> a, std::list<std::list<struct link>::iterator> b);
bool equivalent_list_of_scenarios(std::list<std::list<struct link>::iterator> a, std::list<std::list<struct link>::iterator> b);

// print/display functions

void print_field(int mode, int screen);
void print_gamestats(int screen);

// mult-cell strategies

int strat_121_cross(struct cell * center);
int strat_nonoverlap_safe(struct cell * center);
int strat_nonoverlap_flag(struct cell * center);

// major structural functions (just for encapsulation, each is only called once)

inline int parse_input_args(int margc, char *margv[]);
inline void reset_for_game(std::vector<std::vector<struct cell>> * field_blank);
inline void generate_new_field();
inline int play_game();







// member function definitions


void run_stats::print_final_stats() {
	// calculate total time elapsed and format for display
	std::chrono::milliseconds ms = std::chrono::duration_cast< std::chrono::milliseconds >(
		std::chrono::system_clock::now().time_since_epoch()
		);
	long f = ms.count();
	int elapsed_ms = int(f - start); // currently in ms
	double elapsed_sec = double(elapsed_ms) / 1000.; // seconds with a decimal

	double sec = fmod(elapsed_sec, 60.);
	int elapsed_min = int((elapsed_sec - sec) / 60.);
	int min = elapsed_min % 60;
	int hr = (elapsed_min - min) / 60;
	char timestr[20];
	sprintf_s(timestr, "%i:%02i:%05.3f", hr, min, sec);

	// print/log overall results (always print to terminal and log)
	myprintfn(2, "\nDone playing all %i games, displaying results! Time = %s\n\n", NUM_GAMES_var, timestr);
	myprintfn(2, "MinesweeperSolver version %s\n", VERSION_STRING_def);
	myprintfn(2, "Games used X/Y/mines = %i/%i/%i\n", SIZEX_var, SIZEY_var, NUM_MINES_var);
	if (FIND_EARLY_ZEROS_var) {
		myprintfn(2, "Using 'hunting' method = succeed early (uncover only zeroes until solving can begin)\n");
	} else {
		myprintfn(2, "Used 'hunting' method = human-like (can lose at any stage)\n");
	}
	if (RANDOM_USE_SMART_var) {
		myprintfn(2, "Used 'guessing' mode = smartguess (slower but increased winrate)\n");
		myprintfn(2, "    Smartguess border est. avg deviation:   %+7.4f\n", (smartguess_diff / float(smartguess_attempts)));
		myprintfn(2, "    Times when recursion got out of hand:%5i\n", smartguess_valves_triggered);
	} else {
		myprintfn(2, "Used 'guessing' mode = guess randomly (lower winrate but faster)\n");
	}
	myprintfn(2, "Average time per game:                     %8.4f sec\n", (float(elapsed_sec) / float(games_total)));
	myprintfn(2, "Average 121-cross uses per game:           %5.1f\n", (float(strat_121_total) / float(games_total)));
	myprintfn(2, "Average nonoverlap-flag uses per game:     %5.1f\n", (float(strat_nov_flag_total) / float(games_total)));
	myprintfn(2, "Average nonoverlap-safe uses per game:     %5.1f\n", (float(strat_nov_safe_total) / float(games_total)));
	myprintfn(2, "Average number of guesses per game:        %6.2f + initial guess\n\n", (float(num_guesses_total) / float(games_total)));
	myprintfn(2, "Total games played:                     %6i\n", games_total);
	
	if (games_with_eights != 0) {
		myprintfn(2, "    Games with 8-adj cells:              %5i\n", games_with_eights);
	}
	myprintfn(2, "    Total games won:                     %5i   %5.1f%%    -----\n", games_won, (100. * float(games_won) / float(games_total)));
	myprintfn(2, "        Games won without guessing:      %5i   %5.1f%%   %5.1f%%\n", games_won_noguessing, (100. * float(games_won_noguessing) / float(games_total)), (100. * float(games_won_noguessing) / float(games_won)));
	myprintfn(2, "        Games won that required guessing:%5i   %5.1f%%   %5.1f%%\n", games_won_guessing, (100. * float(games_won_guessing) / float(games_total)), (100. * float(games_won_guessing) / float(games_won)));
	myprintfn(2, "    Total games lost:                    %5i   %5.1f%%    -----\n", games_lost, (100. * float(games_lost) / float(games_total)));
	myprintfn(2, "        Games lost in the first move(s): %5i   %5.1f%%   %5.1f%%\n", games_lost_beginning, (100. * float(games_lost_beginning) / float(games_total)), (100. * float(games_lost_beginning) / float(games_lost)));
	myprintfn(2, "        Games lost early      (1-15%%):   %5i   %5.1f%%   %5.1f%%\n", games_lost_earlygame, (100. * float(games_lost_earlygame - games_lost_beginning) / float(games_total)), (100. * float(games_lost_earlygame - games_lost_beginning) / float(games_lost)));
	myprintfn(2, "        Games lost in midgame (15-85%%):  %5i   %5.1f%%   %5.1f%%\n", games_lost_midgame, (100. * float(games_lost_midgame) / float(games_total)), (100. * float(games_lost_midgame) / float(games_lost)));
	myprintfn(2, "        Games lost in lategame(85-99%%):  %5i   %5.1f%%   %5.1f%%\n", games_lost_lategame, (100. * float(games_lost_lategame) / float(games_total)), (100. * float(games_lost_lategame) / float(games_lost)));
	if (games_lost_unexpectedly != 0) {
		myprintfn(2, "        Games lost unexpectedly:         %5i   %5.1f%%   %5.1f%%\n", games_lost_unexpectedly, (100. * float(games_lost_unexpectedly) / float(games_total)), (100. * float(games_lost_unexpectedly) / float(games_lost)));
	}
	myprintfn(2, "\n");

	fflush(logfile);
}


// return the WEIGHTED average of all the contents that are <= mines_remaining
// also modifies the object by deleting any solutions with answers > mines_remaining
float podwise_return::avg() {
	float a = 0; int total_weight = 0;
	std::list<struct solutionobj>::iterator listit = solutions.begin();
	while (listit != solutions.end()) {
		if (listit->answer <= float(mines_remaining)) { 
			if (USE_WEIGHTED_AVG) {
				a += listit->answer * listit->allocs;
				total_weight += listit->allocs;
			} else {
				a += listit->answer;
				total_weight++;
			}
			listit++;
		} else {
			// erase listit
			std::list<struct solutionobj>::iterator eraseme = listit;
			listit++;
			solutions.erase(eraseme);
		}
	}
	if (total_weight == 0) {
		myprintfn(2, "ERR: IN AVG, ALL SCENARIOS FOUND ARE TOO BIG\n");
		// TODO: decide whether to return 0 or mines_remaining
		return 0.;
		//return float(mines_remaining);
	}
	return (a / float(total_weight));
}
// max: if there is a tie, or the solution doesn't represent only one allocations, return pointer to NULL
struct solutionobj * podwise_return::max() {
	std::list<struct solutionobj>::iterator solit = solutions.begin();
	std::list<struct solutionobj>::iterator maxit = solutions.begin(); // the one to return
	bool tied_for_max = false;
	for (solit++; solit != solutions.end(); solit++) {
		if (solit->answer < maxit->answer)
			continue; // existing max is greater
		if (solit->answer == maxit->answer)
			tied_for_max = true; // tied
		else if(solit->answer > maxit->answer) {
			tied_for_max = false; maxit = solit;
		}
	}
	if (tied_for_max || (maxit->allocs != 1)) { return NULL; } else { return &(*maxit); }
}
// max_val: if there is a tie, return a value anyway
float podwise_return::max_val() {
	float retmax = 0.;
	for (std::list<struct solutionobj>::iterator solit = solutions.begin(); solit != solutions.end(); solit++) {
		if (solit->answer <= retmax)
			continue; // existing max is greater
		if (solit->answer > retmax)
			retmax = solit->answer;
	}
	return retmax;
}
// min: if there is a tie, or the solution doesn't represent only one allocations, return pointer to NULL
struct solutionobj * podwise_return::min() {
	std::list<struct solutionobj>::iterator solit = solutions.begin();
	std::list<struct solutionobj>::iterator minit = solutions.begin(); // the one to return
	bool tied_for_min = false;
	for (solit++; solit != solutions.end(); solit++) {
		if (solit->answer > minit->answer)
			continue; // existing min is lesser
		if (solit->answer == minit->answer)
			tied_for_min = true; // tied
		else if (solit->answer < minit->answer) {
			tied_for_min = false; minit = solit;
		}
	}
	if (tied_for_min || (minit->allocs != 1)) { return NULL; } else { return &(*minit); }
}
// min_val: if there is a tie, return a value anyway
float podwise_return::min_val() {
	float retmin = 100000000.;
	for (std::list<struct solutionobj>::iterator solit = solutions.begin(); solit != solutions.end(); solit++) {
		if (solit->answer >= retmin)
			continue; // existing max is greater
		if (solit->answer < retmin)
			retmin = solit->answer;
	}
	return retmin;
}
// total_alloc: sum the alloc values for all solutions in the list
int podwise_return::total_alloc() {
	int retval = 0;
	for (std::list<struct solutionobj>::iterator solit = solutions.begin(); solit != solutions.end(); solit++) {
		retval += solit->allocs;
	}
	return retval;
}
//bool remove_solution_if_too_big(const struct solutionobj& val) { return (val.answer > mines_remaining); }
//// removes all solutions from the list that are too large (is this necessary? is this helpful?)
//inline void podwise_return::validate() { solutions.remove_if(remove_solution_if_too_big); }


// two links are equivalent if they have the same shared_cell and also their shared_roots compare equal
// needed for remove-by-value
inline bool link::operator== (const struct link o) const {
	if((o.link_cell != link_cell) || (linked_roots.size() != o.linked_roots.size()))
		return false;
	// if they are the same size...
	std::list<struct cell *>::const_iterator ait = linked_roots.begin();
	std::list<struct cell *>::const_iterator bit = o.linked_roots.begin();
	while (ait != linked_roots.end()) {
		if (*ait != *bit)
			return false;
		ait++; bit++;
	}
	// i guess they're identical
	return true;
}

// init from root, mines from root, link_cells empty, cell_list from root
pod::pod(struct cell * new_root) {
	root = new_root;
	mines = new_root->effective;
	links = std::list<struct link>(); // links initialized empty
	cell_list = filter_adjacent(new_root, UNKNOWN); // find adjacent unknowns
	chain_idx = -1; // set later
	cell_list_size = -1; // set later
}
// add a link object to my list of links; doesn't modify cell_list, doesn't modify any other pod
// inserts it into sorted position, so the list of shared_roots will always be sorted
void pod::add_link(struct cell * shared, struct cell * shared_root) {
	// search for link... if found, add new root. if not found, create new.
	std::list<struct link>::iterator link_iter;
	for (link_iter = links.begin(); link_iter != links.end(); link_iter++) {
		if (link_iter->link_cell == shared) {
			// if a match is found, add it to existing
			link_iter->linked_roots.push_back(shared_root);
			std::list<struct cell *>::iterator middle = link_iter->linked_roots.end(); middle--;
			if (link_iter->linked_roots.begin() != middle) {
				std::inplace_merge(link_iter->linked_roots.begin(), middle, link_iter->linked_roots.end(), sort_by_position);
			}
			break;
		}
	}
	if (link_iter == links.end()) {
		// if match is not found, then create new
		links.push_back(link(shared, shared_root));
	}
}
// remove the link from this pod. return 0 if it worked, 1 if the link wasn't found
int pod::remove_link(struct cell * shared, bool isaflag) {
	std::list<struct link>::iterator link_iter;
	for (link_iter = links.begin(); link_iter != links.end(); link_iter++) {
		if (link_iter->link_cell == shared) {
			// found it!
			// always reduce size, one way or another
			if (cell_list_size == 0) { myprintfn(2, "ERR: REMOVING LINK WHEN SIZE ALREADY IS ZERO\n"); } // DEBUG
			if (cell_list_size > 0) {
				cell_list_size--;
			} else {
				int s = cell_list.size();
				int i = 0;
				for (i = 0; i < s; i++) {
					if (cell_list[i] == shared) {
						// when found, delete it by copying the end onto it
						if (i != (s - 1)) { cell_list[i] = cell_list.back(); }
						cell_list.pop_back();
						break;
					}
				}
				if(i == s) { myprintfn(2, "ERR: CAN'T FIND LINK TO REMOVE IN CELL_LIST\n"); } // debug
			}
			if (isaflag) { mines--; }	// only sometimes reduce flags
			links.erase(link_iter);
			return 0;
		}
	}
	// note: won't always find the link, in that case just 'continue'
	return 1;
}

// find all possible ways to allocate mines in the links/internals
std::list<struct scenario> pod::find_scenarios() {
	// return a list of scenarios,
	// where each scenario is a list of ITERATORS which point to my links
	std::list<struct scenario> retme;
	// determine what values K to use, iterate over the range
	int start = mines - (size() - links.size());
	if (start < 0) { start = 0; } // greater of start or 0
	int end = (mines < links.size() ? mines : links.size()); // lesser of mines/links
	for (int i = start; i <= end; i++) {
		// allocate some number of mines among the link cells
		std::list<std::vector<int>> ret = comb(i, links.size());
		// turn the ints into iterators
		std::list<std::list<std::list<struct link>::iterator>> buildme; // where it gets put into
		for (std::list<std::vector<int>>::iterator retit = ret.begin(); retit != ret.end(); retit++) {
			// for each scenario...
			buildme.push_back(std::list<std::list<struct link>::iterator>()); // push an empty list of iterators
			for (int k = 0; k < i; k++) { // each scenario will have length i
				buildme.back().push_back(get_link((*retit)[k]));
			}
			// sort the scenario blind-wise so that equiv links are adjacent
			buildme.back().sort(sort_scenario_blind);
		}

		// now 'ret' has been converted to 'buildme', list of sorted lists of link-list-iterators
		buildme.sort(sort_list_of_scenarios);
		// now I have to do my own uniquify-and-also-build-scenario-objects section
		std::list<struct scenario> retme_sub;
		std::list<std::list<std::list<struct link>::iterator>>::iterator buildit = buildme.begin();
		std::list<std::list<std::list<struct link>::iterator>>::iterator prevlist = buildit;
		int c = comb_int((mines - i), (size() - links.size()));
		retme_sub.push_back(scenario((*buildit), c));

		for (buildit++; buildit != buildme.end(); buildit++) {
			if (compare_list_of_scenarios(*prevlist, *buildit) == 0) {
				// if they are the same, revise the previous entry
				retme_sub.back().allocs += c;
			} else {
				// if they are different, add as a new entry
				retme_sub.push_back(scenario((*buildit), c));
			}
			prevlist = buildit;
		}
		// add retme_sub to the total string retme
		retme.insert(retme.end(), retme_sub.begin(), retme_sub.end());
	}
	return retme;
}

// when given an int, retrieve the link at that index
// PRIVATE! for internal use only
std::list<struct link>::iterator pod::get_link(int l) {
	std::list<struct link>::iterator link_iter = links.begin();
	for (int i = 0; i < l; i++) { link_iter++; }
	return link_iter;
}

link::link(struct cell * shared, struct cell * shared_root) {
	link_cell = shared;
	linked_roots.push_back(shared_root);
}

chain::chain() {
	podlist = std::list<struct pod>(); // explicitly init as an empty list
}
// find a pod with the given root, return iterator so it's easier to delete or whatever
std::list<struct pod>::iterator chain::root_to_pod(struct cell * linked_root) {
	if (linked_root == NULL)
		return podlist.end();
	std::list<struct pod>::iterator pod_iter;
	for (pod_iter = podlist.begin(); pod_iter != podlist.end(); pod_iter++) {
		if (pod_iter->root == linked_root)
			return pod_iter;
	}
	// I wanna return NULL but that's not an option...
	// is this line possible? is it not? I CAN'T REMEMBER
	return podlist.end(); // note this actually points to the next open position; there's no real element here!
}
// returns iterator to the fth pod
std::list<struct pod>::iterator chain::int_to_pod(int f) {
	if (f > podlist.size()) { return podlist.end(); }
	std::list<struct pod>::iterator iter = podlist.begin();
	for (int i = 0; i < f; i++) { iter++; }
	return iter;
}
// RECURSE and mark connected chains/islands... should only be called outside of recursion, when it is a "master chain"
// assumes that links have already been set up, duplicates removed, and subtraction performed, etc
// returns the number of disjoint chains found (returns 4 if chains have id 0/1/2/3) (returns 1 if already one contiguous chain, id 0)
int chain::identify_chains() {
	// first, reset any chain indices (to allow for calling it again inside recursion)
	for (std::list<struct pod>::iterator pod_iter = podlist.begin(); pod_iter != podlist.end(); pod_iter++) {
		pod_iter->chain_idx = -1;
	}
	// then, do the actual deed
	int next_chain_idx = 0;
	for (std::list<struct pod>::iterator pod_iter = podlist.begin(); pod_iter != podlist.end(); pod_iter++) {
		if (pod_iter->chain_idx == -1) {
			identify_chains_recurse(next_chain_idx, &(*pod_iter));
			next_chain_idx++;
		}
	}
	return next_chain_idx;
}
// PRIVATE! internal use only
void chain::identify_chains_recurse(int idx, struct pod * me) {
	if (me->chain_idx != -1) {
		assert(me->chain_idx == idx); // if its not -1 then it damn well better be idx
		return; // this pod already marked by something (hopefully this same recursion call)
	}
	me->chain_idx = idx; // paint it
	for (std::list<struct link>::iterator linkit = me->links.begin(); linkit != me->links.end(); linkit++) {
		// for each link object 'linkit' in me...
		for (std::list<struct cell *>::iterator rootit = linkit->linked_roots.begin(); rootit != linkit->linked_roots.end(); rootit++) {
			// for each root pointer 'rootit' in linkit...
			// ...get the pod that is linked...
			struct pod * linked_pod = &(*(root_to_pod(*rootit)));
			// ...and recurse on that pod with the same index
			identify_chains_recurse(idx, linked_pod);
		}
	}
	return;
}
// after calling identify_chains, sort the pods into a vector of chains
// if 'reduce' is true, then also turn "cell_list" into a simple number, and clear the actual list (only for before recursion!)
std::vector<struct chain> chain::sort_into_chains(int r, bool reduce) {
	std::vector<struct chain> chain_list; // where i'll be sorting them into
	std::vector<struct cell *> blanklist = std::vector<struct cell *>();
	chain_list.resize(r, chain());
	for (std::list<struct pod>::iterator podit = podlist.begin(); podit != podlist.end(); podit++) {
		int v = podit->chain_idx;
		chain_list[v].podlist.push_back(*podit);
		if (reduce) {
			chain_list[v].podlist.back().cell_list_size = chain_list[v].podlist.back().cell_list.size();
			chain_list[v].podlist.back().cell_list = blanklist;
		}
	}
	return chain_list;
}

// constructor
riskholder::riskholder(int x, int y) {
	struct riskcell asdf2 = riskcell(); // needed for using the 'resize' command
	std::vector<struct riskcell> asdf = std::vector<struct riskcell>(); // init empty
	asdf.resize(y, asdf2);										// fill it with copies of asdf2
	asdf.shrink_to_fit();												// set capacity exactly where i want it, no bigger
	riskarray = std::vector<std::vector<struct riskcell>>();// init empty
	riskarray.resize(x, asdf);						// fill it with copies of asdf
	riskarray.shrink_to_fit();								// set capacity exactly where i want it, no bigger
}
// iterate over itself and return the stuff tied for lowest risk
struct riskreturn riskholder::findminrisk() {
	std::list<struct cell *> minlist = std::list<struct cell *>();
	float minrisk = 100.;
	for (int m = 0; m < SIZEX_var; m++) {
		for (int n = 0; n < SIZEY_var; n++) {
			float j = finalrisk(m, n);
			if ((j == -1.) || (j > minrisk))
				continue;
			if (j < minrisk) {
				minrisk = j;
				minlist.clear();
			}
			minlist.push_back(&field[m][n]);
		}
	}
	struct riskreturn retme = riskreturn(minrisk, &minlist);
	return retme;
}










// other function definitions


// cellptr: if the given X and Y are valid, returns a pointer to the cell; otherwise returns NULL
// during single-cell and multi-cell iterations, just use &field[x][y] because the X and Y are guaranteed not off the edge
inline struct cell * cellptr(int x, int y) {
	if ((x < 0) || (x >= SIZEX_var) || (y < 0) || (y >= SIZEY_var))
		return NULL;
	return &field[x][y];
}

// adjacent: returns 3/5/8 adjacent cells, regardless of content or status
// the order of the cells in the list will always be the same!!
std::vector<struct cell *> get_adjacent(struct cell * me) {
	std::vector<struct cell *> adj_list;
	adj_list.reserve(8); // resize it once since this will almost always be size 8
	int x = me->x;
	int y = me->y;
	struct cell * z;
	for (int b = -1; b < 2; b++) {
		for (int a = -1; a < 2; a++) {
			if (a == 0 && b == 0)
				continue; // i am not adjacent to myself
			if ((z = cellptr(x + a, y + b)) != NULL) // simultaneous store and check for null
				adj_list.push_back(z);
		}
	}

	return adj_list;
}


// reveal: recursive function, returns -1 if loss or the # of cells revealed otherwise
// if it's a zero, remove it from the zero-list and recurse
// doesn't complain if you give it a null pointer
int reveal(struct cell * revealme) {
	if (revealme == NULL) return 0;
	if (revealme->status != UNKNOWN)
		return 0; // if it is flagged, satisfied, or already visible, then do nothing. will happen often.
	
	unklist.remove(revealme);
	if (revealme->value == MINE) {
		// lose the game, handled wherever calls here
		revealme->status = VISIBLE;
		return -1;
	} else if(revealme->effective == 0) {
		revealme->status = SATISFIED;
		// if it's a zero, recurse
		// also recurse if the cell has already been satisfied; not just true zeroes
		if(revealme->value == 0) {
			//if a true zero, remove it from the zero-list
			zerolist.remove(revealme);
		}
		int retme = 0;
		int t = 0;
		std::vector<struct cell *> adj = filter_adjacent(revealme, UNKNOWN);
		std::vector<struct cell *>::iterator ptr;
		for (ptr = adj.begin(); ptr < adj.end(); ptr++) {
			// ptr is a pointer to a pointer to a cell, therefore it needs dereferenced once
			t = reveal(*ptr);
			if (t == -1) {
				return -1;
			} else {
				retme += t;
			}
		}
		return retme + 1; // return the number of cells revealed
	} else {
		// if its an adjacency number, return 1
		revealme->status = VISIBLE;
		return 1;
	}
	return 0; // dangling return just in case
}

// flag: sets as flagged, reduces # remaining mines, reduce "effective" values of everything around it (regardless of status)
// if remaining_mines = 0, validate the win; if it passes, return 1 for win! if it fails, something went wrong!
// if remaining_mines != 0, return 0 and continue solving
// if not an actual mine, dump a bunch of info and abort
int set_flag(struct cell * flagme) {
	if (flagme->status != UNKNOWN) {
		return 0;
	}

	flagme->status = FLAGGED;
	unklist.remove(flagme);

	// check if it flagged a non-mine square, and if so, why?
	if (flagme->value != MINE) {
		myprintfn(2, "ERR: FLAGGED A NON-MINE CELL\n");
		//assert(flagme->value == MINE);
		//system("pause");
	}

	// reduce "effective" values of everything around it, whether its visible or not
	std::vector<struct cell *> adj = get_adjacent(flagme);
	for (int i = 0; i < adj.size(); i++) {
		if(adj[i]->value != MINE)
			adj[i]->effective -= 1;
	}

	// decrement the remaining mines
	mines_remaining -= 1;
	if (mines_remaining == 0) {
		//try to validate the win: every mine is flagged, and every not-mine is not-flagged
		for (int y = 0; y < SIZEY_var; y++) {for (int x = 0; x < SIZEX_var; x++) { // iterate over each cell
			struct cell * v = &field[x][y];
			if (v->value == MINE) {
				// assert that every mine is flagged
				if (v->status != FLAGGED) {
					myprintfn(2, "ERR: IN WIN VALIDATION, FOUND AN UNFLAGGED MINE\n");
					assert(v->status == FLAGGED);
				}
			}
			if (v->value != MINE) {
				// assert that every not-mine is not-flagged
				if (v->status == FLAGGED) {
					myprintfn(2, "ERR: IN WIN VALIDATION, FOUND A FLAGGED NOT-MINE\n");
					assert(v->status != FLAGGED);
				}
			}
		}}
		// officially won!
		return 1;
	}
	
	return 0;
}

// filter_adjacent: basically the same as get_adjacent but returns only the TARGET cells
// can plausibly return an empty vector
// this version takes a cell pointer
std::vector<struct cell *> filter_adjacent(struct cell * me, cell_state target) {
	std::vector<struct cell *> adj = get_adjacent(me);
	return filter_adjacent(adj, target);
}


// filter_adjacent: basically the same as get_adjacent but returns only the TARGET cells
// can plausibly return an empty vector
// this version takes an adjacency vector
std::vector<struct cell *> filter_adjacent(std::vector<struct cell *> adj, cell_state target) {
	int i = 0;
	while (i < adj.size()) {
		if (adj[i]->status != target)
			adj.erase(adj.begin() + i);
		else
			i++;
	}
	return adj;
}


// find_nonoverlap: takes two vectors of cells, returns (first_vect_unique) (second_vect_unique) (overlap)
std::vector<std::vector<struct cell *>> find_nonoverlap(std::vector<struct cell *> me_unk, std::vector<struct cell *> other_unk) {
	std::vector<struct cell *> overlap = std::vector<struct cell *>();
	for (int i = 0; i != me_unk.size(); i++) { // for each cell in me_unk...
		for (int j = 0; j != other_unk.size(); j++) { // ... compare against each cell in other_unk...
			if (me_unk[i] == other_unk[j]) {// ... until there is a match!
				overlap.push_back(me_unk[i]);
				me_unk.erase(me_unk.begin() + i);
				other_unk.erase(other_unk.begin() + j); // not sure if this makes it more or less efficient...
				i--; // need to counteract the i++ that happens in outer loop
				break;
			}
		}
	}
	std::vector<std::vector<struct cell *>> retme;
	retme.push_back(me_unk);
	retme.push_back(other_unk);
	retme.push_back(overlap);
	return retme;
}


// print: either 1) fully-revealed field, 2) in-progress field as seen by human, 3) in-progress field showing 'effective' values
// borders made with +, zeros: blank, adjacency (or effective): number, unknown: -, flag or mine: *
// when SCREEN_var is low enough, skipped entirely (doesn't even print to log)
void print_field(int mode, int screen) {
	if (screen > 0) {
		if (mode == 1) {
			myprintfn(screen, "Printing fully-revealed field\n")
		} else if (mode == 2) {
				myprintfn(screen, "Printing in-progress field seen by humans\n");
			} else if (mode == 3) {
				myprintfn(screen, "Printing in-progress field showing effective values\n");
			}
			// build and print the field one row at a time

			// top/bottom:
			std::string top = std::string(((SIZEX_var + 2) * 2) - 1, '+') + "\n";
			myprintfn(screen, top.c_str());

			std::string line;
			for (int y = 0; y < SIZEY_var; y++) {
				for (int x = 0; x < SIZEX_var; x++) {
					if ((field[x][y].status == UNKNOWN) && (mode != 1)) {
						// if mode==full, fall through
						line += "- "; continue;
					}
					// if visible and a mine, this is where the game was lost
					if ((field[x][y].status == VISIBLE) && (field[x][y].value == MINE)) {
						line += "X "; continue;
					}
					// below here assume the cell is visible/flagged/satified
					if ((field[x][y].status == FLAGGED) || (field[x][y].value == MINE)) {
						line += "* "; continue;
					}
					// below here assumes the cell is a visible adjacency number
					if ((field[x][y].value == 0) || ((mode == 3) && (field[x][y].effective == 0))) {
						line += "  "; continue;
					}
					char buf[3];
					if (mode == 3)
						sprintf_s(buf, "%d", field[x][y].effective);
					else
						sprintf_s(buf, "%d", field[x][y].value);
					line += buf;
					line += " ";
				}

				myprintfn(screen, "+ %s+\n", line.c_str());
				line.clear();
			}

			myprintfn(screen, (top + "\n").c_str());
			fflush(logfile);
	}
}


// print the stats of the current game, and some whitespace below
void print_gamestats(int screen) {
	myprintfn(screen, "Transition map: %s\n", mygamestats.trans_map.c_str());
	myprintfn(screen, "121-cross hits: %i, nonoverlap-safe hits: %i, nonoverlap-flag hits: %i\n",
		mygamestats.strat_121, mygamestats.strat_nov_safe, mygamestats.strat_nov_flag);
	myprintfn(screen, "Cells guessed: %i\n", mygamestats.times_guessing);
	myprintfn(screen, "Flags placed: %i / %i\n\n\n", NUM_MINES_var - mines_remaining, NUM_MINES_var);
	fflush(logfile);
}


// determine all ways to choose K from N, return a VECTOR of VECTORS of INTS
// result is a list of lists of indices from 0 to N-1
// ex: comb(3,5)
std::list<std::vector<int>> comb(int K, int N) {
	// thanks to some dude on StackExchange
	std::list<std::vector<int>> buildme;
	if (N == 0) 
		return buildme;
	static std::vector<bool> bitmask = std::vector<bool>();
	bitmask.resize(K, 1);	// a vector of K leading 1's...
	bitmask.resize(N, 0);	// ... followed by N-K trailing 0's, total length = N
							// permutate the string of 1s and 0s and see what happens
	do {
		buildme.push_back(std::vector<int>(K,-1)); // start a new entry, know it will have size K
		int z = 0;
		//buildme->back().reserve(K); // either just the same efficiency, or slightly better
		for (int i = 0; i < N; ++i) {// for each index, is the bit at that index a 1? if yes, choose that item
			if (bitmask[i]) { buildme.back()[z] = i; z++; }
		}
	} while (std::prev_permutation(bitmask.begin(), bitmask.end()));
	bitmask.clear(); //hopefully doesn't reduce capacity, that would undermine the whole point of static memory!
	return buildme;
}

// return the # of total combinations you can choose K from N, simple math function
// currently only used for extra debug info
// = N! / (K! * (N-K)!)
inline int comb_int(int K, int N) {
	return factorial(N) / (factorial(K) * factorial(N - K));
}

inline int factorial(int x) {
	if (x < 2) return 1;
	switch (x) {
	case 2: return 2;
	case 3: return 6;
	case 4: return 24;
	case 5: return 120;
	case 6: return 720;
	case 7: return 5040;
	case 8: return 40320;
	default: return -1;
	}
}


// return a random cell from the provided list, or NULL if the list is empty
struct cell * rand_from_list(std::list<struct cell *> * fromme) {
	if ((*fromme).empty()) {
		return NULL;
	}
	int f = rand() % (*fromme).size();

	std::list<struct cell *>::iterator iter = (*fromme).begin();
	for (int i = 0; i < f; i++) {
		iter++;
	}
	return *iter;
}



// laboriously determine the % risk of each unknown cell and choose the one with the lowest risk to return
// can completely solve the puzzle, too; therefore it can return multiple cells to clear or to flag
struct smartguess_return smartguess() {
	struct smartguess_return results;
	// ROADMAP
	// 1)build the pods from visible, allow for dupes, no link cells yet. is stored in the "master chain". don't modify interior_unk yet
	// 2)iterate over pods, check for dupes and subsets (call find_nonoverlap on each pod with root in 5x5 around my root)
		// note if a pod becomes 100% or 0%, loop until no changes happen
	// 3)if any pods became 100% or 0%, return with those
	// 4)iterate again, removing pod contents from 'interior_unk'. Done here so there are fewer dupe pods, less time searching thru interior_unk
		
	// if interior_unk is empty, then skip the following:
	// 5)iterate again, building links to anything within 5x5(only for ME so there are no dupes)
	// 6)iterate/recurse, identifying chains
	// iterate AGAIN, separating them into a VECTOR of chains (set size beforehand so i can index into the vector)
		// at the same time, turn "cell_list" into a simple number, and clear the actual list???
		// create an 'empty list' with size=0, capacity=0, and copy it onto each cell_list so recursing uses less memory
	// 7)for each chain, recurse (chain is only argument) and get back max/min for that chain
	// !!!!!!!!!!!!!!!!!!!!!
	// sum all the max/min, concat to # of mines remaining, average the two (perhaps do something fancier? dunno)
	// 8)calculate interior_risk

	// 9)iterate over "master chain", storing risk information into 'riskholder' (only read cell_list since it also holds the links)
	// 10)find the minimum risk from anything in the border cells (pods) and any cells with that risk
	// 11)decide between border and interior, call 'rand_from_list' and return


	struct chain master_chain; // list of pods
	std::list<struct cell *> interior_list = unklist;

	// step 1: iterate over field, get 'visible' cells, use them as roots to build pods.
	// non-optimized: includes duplicates, before pod-subtraction. interior_unk not yet modified.
	// pods are added to the chain already sorted (reading order by root), as are their cell_list contents
	for (int y = 0; y < SIZEY_var; y++) {for (int x = 0; x < SIZEX_var; x++) { // iterate over each cell
		if (field[x][y].status == VISIBLE) {
			master_chain.podlist.push_back(pod(&field[x][y])); // constructor gets adj unks for the given root
		}
	}}

	// step 2: iterate over pods, check for dupes and subsets (call find_nonoverlap on each pod with root in 5x5 around my root)
	// note if a pod becomes 100% or 0%... repeat until no changes are done.
	// when deleting pods, what remains will still be in sorted order
	bool changes;
	do {
		changes = false;
		for (std::list<struct pod>::iterator podit = master_chain.podlist.begin(); podit != master_chain.podlist.end(); podit++) {
			// to find pods that may be dupes or subsets, use one of the following methods:
			for (int b = -2; b < 3; b++) {for (int a = -2; a < 3; a++) {
				if (((a == -2 || a == 2) && (b == -2 || b == 2)) || (a == 0 && b == 0)) { continue; } // skip myself and also the corners
				struct cell * other = cellptr(podit->root->x + a, podit->root->y + b);
				if ((other == NULL) || (other->status != VISIBLE)) { continue; } // only examine the visible cells
				std::list<struct pod>::iterator otherpod = master_chain.root_to_pod(other);
				if (otherpod == master_chain.podlist.end()) { continue; }
				// for each pod 'otherpod' with root within 5x5 found...
				std::vector<std::vector<struct cell *>> n = find_nonoverlap(podit->cell_list, otherpod->cell_list);
				if (n[0].empty()) {
					changes = true;
					if (n[1].empty()) {
						// is it a total duplicate? if yes, delete OTHER (not me), OTHER is somewhere later in the list
						master_chain.podlist.erase(otherpod);
					} else {
						// if podit has no uniques but otherpod does, then podit is subset of otherpod... SUBTRACT from otherpod, leave podit unchanged
						otherpod->mines -= podit->mines; otherpod->cell_list = n[1];
						// if a pod would become 100 or 0, otherpod would do it here. is it possible to get dupes if I check it here?...
						float z = otherpod->risk();
						if (z == 0.) {
							// add all cells in otherpod to resultlist[0]
							results.clearme.insert(results.clearme.end(), otherpod->cell_list.begin(), otherpod->cell_list.end());
						} else if (z == 100.) {
							// add all cells in otherpod to resultlist[1]
							results.flagme.insert(results.flagme.end(), otherpod->cell_list.begin(), otherpod->cell_list.end());
						}
					}
				}
			}}
				
		} // end for each pod
	} while (changes);

	// step 3: if any pods were found to be 100 or 0 when optimizing, just return them now
	if (!results.empty()) {
		if(ACTUAL_DEBUG) myprintfn(2, "DEBUG: in smart-guess, optimization found %i clear and %i flag\n", results.clearme.size(), results.flagme.size());
		results.method = 0;
		return results;
	}

	// step 4: now that master_chain is refined, remove pod contents (border unk) from interior_unk
	// will still have some dupes (link cells) but much less than if I did this earlier
	for (std::list<struct pod>::iterator podit = master_chain.podlist.begin(); podit != master_chain.podlist.end(); podit++) {
		for (int i = 0; i < podit->size(); i++) {
			interior_list.remove(podit->cell_list[i]); // remove from interior_list by value (find and remove)
		}
	}

	float interior_risk = 150.;
	if (!interior_list.empty() || (mines_remaining <= RETURN_ACTUAL_ALLOCATION_IF_THIS_MANY_MINES_REMAIN)) {
		// step 5: iterate again, building links to anything within 5x5(only set MY links)
		for (std::list<struct pod>::iterator podit = master_chain.podlist.begin(); podit != master_chain.podlist.end(); podit++) {
			for (int b = -2; b < 3; b++) {for (int a = -2; a < 3; a++) {
				if (a == 0 && b == 0) { continue; } // skip myself BUT INCLUDE THE CORNERS
				struct cell * other = cellptr(podit->root->x + a, podit->root->y + b);
				if ((other == NULL) || (other->status != VISIBLE)) { continue; } // only examine the visible cells
				std::list<struct pod>::iterator otherpod = master_chain.root_to_pod(other);
				if (otherpod == master_chain.podlist.end()) { continue; } // some pods will have been optimized away
				// for each pod 'otherpod' with root within 5x5 found...
				std::vector<std::vector<struct cell *>> n = find_nonoverlap(podit->cell_list, otherpod->cell_list);
				// ... only create links within podit!
				for (int i = 0; i < n[2].size(); i++) {
					podit->add_link(n[2][i], otherpod->root);
				}
			}}
		}

		// step 6: identify chains and sort the pods into a VECTOR of chains... 
		// at the same time, turn "cell_list" into a simple number, and clear the actual list so it uses less memory while recursing
		int numchains = master_chain.identify_chains();
		// if there are only a few mines left, then don't eliminate the cell_list contents
		bool reduce = (mines_remaining > RETURN_ACTUAL_ALLOCATION_IF_THIS_MANY_MINES_REMAIN);
		std::vector<struct chain> listofchains = master_chain.sort_into_chains(numchains, reduce);

		// step 7: for each chain, recurse (depth, chain are only arguments) and get back list of answer allocations
		// handle the multiple podwise_retun objects, just sum their averages
		if(ACTUAL_DEBUG) myprintfn(2, "DEBUG: in smart-guess, # primary chains = %i \n", listofchains.size());
		float border_allocation = 0;
		std::vector<struct podwise_return> retholder = std::vector<struct podwise_return>(numchains, podwise_return());
		for (int s = 0; s < numchains; s++) {
			recursion_safety_valve = false; // reset the flag for each chain
			struct podwise_return asdf = podwise_recurse(CHAIN_RECHECK_DEPTH, listofchains[s]);
			border_allocation += asdf.avg();
			if (mines_remaining <= RETURN_ACTUAL_ALLOCATION_IF_THIS_MANY_MINES_REMAIN) { retholder[s] = asdf; }
			if (ACTUAL_DEBUG || recursion_safety_valve) myprintfn(2, "DEBUG: in smart-guess, chain %i with %i pods found %i solutions\n", s, listofchains[s].podlist.size(), asdf.size());
			if (ACTUAL_DEBUG) myprintfn(2, "DEBUG: in smart-guess, chain %i eliminated %.3f%% of allocations as redundant\n", s, (100. * asdf.efficiency()));
			myrunstats.smartguess_valves_triggered += recursion_safety_valve;
		}


		// step 8: if near the endgame, check if any solutions fit perfectly
		if (mines_remaining <= RETURN_ACTUAL_ALLOCATION_IF_THIS_MANY_MINES_REMAIN) {
			float minsum = 0; float maxsum = 0;
			for (int a = 0; a < numchains; a++) { minsum += retholder[a].min_val(); maxsum += retholder[a].max_val(); }

			if (minsum == mines_remaining) {
				// int_list is safe
				results.clearme.insert(results.clearme.end(), interior_list.begin(), interior_list.end());
				// for each chain, get the minimum solution object (if there is only one), and if it has only 1 allocation, then apply it as flags
				for (int a = 0; a < numchains; a++) {
					struct solutionobj * minsol = retholder[a].min();
					if (minsol != NULL)
						results.flagme.insert(results.flagme.end(), minsol->solution_flag.begin(), minsol->solution_flag.end());
				}
			} else if ((maxsum + interior_list.size()) == mines_remaining) {
				// int_list is all mines
				results.flagme.insert(results.flagme.end(), interior_list.begin(), interior_list.end());
				// for each chain, get the maximum solution object (if there is only one), and if it has only 1 allocation, then apply it
				for (int a = 0; a < numchains; a++) {
					struct solutionobj * maxsol = retholder[a].max();
					if (maxsol != NULL)
						results.flagme.insert(results.flagme.end(), maxsol->solution_flag.begin(), maxsol->solution_flag.end());
				}
			}
			if (!results.empty()) {
				if (ACTUAL_DEBUG) myprintfn(2, "DEBUG: in smart-guess, found a definite solution!\n");
				results.method = 2;
				return results;
			}
		}



		// step 9: calculate interior_risk, do statistics things

		// calculate the risk of a mine in any 'interior' cell (they're all the same)
		if (interior_list.empty())
			interior_risk = 150.;
		else {

			////////////////////////////////////////////////////////////////////////
			// DEBUG/STATISTICS STUFF
			// question: how accurate is the count of the border mines? are the limits making the algorithm get a wrong answer?
			// answer: compare 'border_allocation' with a total omniscient count of the mines in the border cells
			// note: easier to find border mines by (bordermines) = (totalmines) - (interiormines)
			int interiormines = 0;
			for (std::list<struct cell *>::iterator cellit = interior_list.begin(); cellit != interior_list.end(); cellit++) {
				if ((*cellit)->value == MINE)
					interiormines++;
			}
			int bordermines = mines_remaining - interiormines;

			if (ACTUAL_DEBUG) myprintfn(2, "DEBUG: in smart-guess, border_avg/ceiling/border_actual = %.3f / %i / %i\n", border_allocation, mines_remaining, bordermines);
			// TODO: change this (and printout at end) to only accumulate when the difference is not 0? would ignore many early-game guessing things
			myrunstats.smartguess_attempts++;
			myrunstats.smartguess_diff += (border_allocation - float(bordermines)); // accumulating a less-negative number
			////////////////////////////////////////////////////////////////////////


			if (border_allocation > mines_remaining) { border_allocation = mines_remaining; }
			interior_risk = (float(mines_remaining) - border_allocation) / float(interior_list.size()) * 100.;
		}

	} // end of finding-likely-border-mine-allocation-to-determine-interior-risk section

	// now, find risk for each border cell from the pods
	// step 10: iterate over "master chain", storing risk information into 'riskholder' (only read cell_list since it also holds the links)
	for (std::list<struct pod>::iterator podit = master_chain.podlist.begin(); podit != master_chain.podlist.end(); podit++) {
		float podrisk = podit->risk();
		for (int i = 0; i < podit->cell_list.size(); i++) {
			myriskholder.addrisk(podit->cell_list[i], podrisk);
		}
	}

	// step 11: find the minimum risk from anything in the border cells (pods) and any cells with that risk
	struct riskreturn myriskreturn = myriskholder.findminrisk();

	// step 12: decide between border and interior, call 'rand_from_list' and return
	if (ACTUAL_DEBUG) myprintfn(2, "DEBUG: in smart-guess, interior_risk = %.3f, border_risk = %.3f\n", interior_risk, myriskreturn.minrisk);
	//if (interior_risk == 100.) {
	//	// unlikely but plausible (note: if interior is empty, interior_risk will be set at 150)
	//	// this is eclipsed by the 'maxguess' section, unless this scenario happens before the end-game
	//	results.flagme.insert(results.flagme.end(), interior_list.begin(), interior_list.end());
	//	results.method = 2;
	//} else 
	if (interior_risk < myriskreturn.minrisk) {
		// interior is safer
		results.clearme.push_back(rand_from_list(&interior_list));
		results.method = 1;
	} else {
		// border is safer, or they are tied
		results.clearme.push_back(rand_from_list(&myriskreturn.minlist));
		results.method = 1;
	}

	return results;

}


// recursively operates on a interconnected 'chain' of pods, and returns the list of all allocations it can find.
// 'rescan_counter' will periodically check that the chain is still completely interlinked; if it has divided, then each section
//		can have the recursion called on it (much faster this way). not sure what frequency it should rescan for chain integrity; 
//		currently set to every 3 levels.
// 'recursion_safety_valve' is set whenever it would return something resulting from 10k or more solutions, then from that point
//		till the chain is done, it checks max 2 scenarios per level.
struct podwise_return podwise_recurse(int rescan_counter, struct chain mychain) {
	// step 0: are we done?
	if (mychain.podlist.empty()) {
		return podwise_return(0, 1);
	} else if (mychain.podlist.size() == 1) {
		int m = mychain.podlist.front().mines;
		int c = comb_int(m, mychain.podlist.front().size());
		return podwise_return(m, c);
	}

	// step 1: is it time to rescan the chain?
	if (rescan_counter > 0) {
		rescan_counter--;
	} else {
		rescan_counter = CHAIN_RECHECK_DEPTH;
		int r = mychain.identify_chains();
		if (r > 1) {
			// if it has broken into 2 or more distinct chains, seperate them to operate recursively on each! much faster than the whole
			std::vector<struct chain> chain_list = mychain.sort_into_chains(r, false); // where i'll be sorting them into

			// handle the multiple podwise_return objects, just sum their averages and total lengths
			// NOTE: this same structure/method used at highest-level, when initially invoking the recursion
			float sum = 0;
			int effort_sum = 0;
			int alloc_product = 1;
			for (int s = 0; s < r; s++) {
				struct podwise_return asdf = podwise_recurse(rescan_counter, chain_list[s]);
				//asdf.validate();
				sum += asdf.avg();
				effort_sum += asdf.effort;
				alloc_product *= asdf.total_alloc();
			}
			
			struct podwise_return asdf2 = podwise_return(sum, alloc_product);
			asdf2.effort = effort_sum;
			if (effort_sum > RECURSION_SAFETY_LIMITER)
				recursion_safety_valve = true;
			return asdf2;
		}
		// if r == 1, then we're still in one contiguous chain. just continue as planned.
	}

	// TODO: is it more efficient to go from the middle (rand) or go from one end (begin) ? results are the same, just a question of time
	//int mainpodindex = 0;
	int mainpodindex = rand() % mychain.podlist.size();
	std::list<struct pod>::iterator temp = mychain.int_to_pod(mainpodindex);
	// NOTE: if there are no link cells, then front() is disjoint! handle appropriately and recurse down, then return (no branching)
	// AFAIK, this should never happen... top-level call on disjoint will be handled step 0, any disjoint created will be handled step 3c
	if (temp->links.empty()) {
		int f = temp->mines;
		mychain.podlist.erase(temp);
		struct podwise_return asdf = podwise_recurse(rescan_counter, mychain);
		asdf += f;
		return asdf;
	}

	// step 2: pick a pod, find all ways to saturate it
	// NOTE: first-level optimization is that only link/overlap cells are considered... specific allocations within interior cells are
	//       ignored, since they have no effect on placing flags in any other pods.
	// NOTE: second-level optimization is eliminating redundant combinations of 'equivalent links', i.e. links that span the same pods
	//       are interchangeable, and I don't need to test all combinations of them.
	// list of lists of list-iterators which point at links in the link-list of front() (that's a mouthfull)
	std::list<struct scenario> scenarios = temp->find_scenarios();

	// where results from each scenario recursion are accumulated
	struct podwise_return retval = podwise_return();
	int whichscenario = 1; // int to count how many scenarios i've tried
	for (std::list<struct scenario>::iterator scit = scenarios.begin(); scit != scenarios.end(); scit++) {
		// for each scenario found above, make a copy of mychain, then...
		int flagsthislvl = 0;
		int allocsthislvl = scit->allocs;
		std::list<struct cell *> cellsflaggedthislvl;
		struct chain copychain = mychain;
		struct podwise_return asdf;

		// these aren't "links", i'm just re-using the struct as a convenient way to couple
		// one link-cell cell with each pod it needs to be removed from
		std::list<struct link_with_bool> links_to_flag_or_clear;

		std::list<struct pod>::iterator frontpod = copychain.int_to_pod(mainpodindex);
		// step 3a: apply the changes according to the scenario by moving links from front() pod to links_to_flag_or_clear
		for(std::list<std::list<struct link>::iterator>::iterator r = scit->roadmap.begin(); r != scit->roadmap.end(); r++) {
			// turn iterator-over-iterator into just an iterator
			std::list<struct link>::iterator linkit = *r;
			// copy it from front().links to links_to_flag_or_clear as 'flag'...
			links_to_flag_or_clear.push_back(link_with_bool(*linkit, true));
			// I really wanted to use "erase" here instead of "remove", oh well
			// linkit is pointing at the link in the pod in the chain BEFORE I made a copy
			// i made links before I made the copy because I found the 'equivalent links' before the copy
			// ...and delete the original
			frontpod->links.remove(*linkit);
		}
		// then copy all remaining links to links_to_flag_or_clear as 'clear'
		for (std::list<link>::iterator linkit = frontpod->links.begin(); linkit != frontpod->links.end(); linkit++) {
			links_to_flag_or_clear.push_back(link_with_bool(*linkit, false));
		}

		// frontpod erases itself and increments by how many mines it has that aren't in the queue
		flagsthislvl += frontpod->mines - scit->roadmap.size();
		copychain.podlist.erase(frontpod);


		// step 3b: iterate along links_to_flag_or_clear, removing the links from any pods they appear in...
		// there is high possibility of duplicate links being added to the list, but its handled just fine
		// decided to run depth-first (stack) instead of width-first (queue) because of the "looping problem"
		while(!links_to_flag_or_clear.empty()) {
			// for each link 'blinkit' to be flagged...
			struct link_with_bool blink = links_to_flag_or_clear.front();
			links_to_flag_or_clear.pop_front();

			struct cell * sharedcell = blink.l.link_cell;
			bool tmp = false;
			// ...go to every pod with a root 'rootit' referenced in 'linkitf' and remove the link from them!
			for (std::list<struct cell *>::iterator rootit = blink.l.linked_roots.begin(); rootit != blink.l.linked_roots.end(); rootit++) {
				std::list<struct pod>::iterator activepod = copychain.root_to_pod(*rootit);
				if (activepod == copychain.podlist.end()) { continue; }
				if (activepod->remove_link(sharedcell, blink.flagme)) { continue; }
				tmp = blink.flagme;

				// step 3c: after each change, see if activepod has become 100/0/disjoint/negative... if so, modify podlist and links_to_flag_or_clear
				//		when finding disjoint, delete it and inc "flags this scenario" accordingly
				//		when finding 100/0, store the link-cells, delete it, and inc "flags this scenario" accordingly
				//		when finding pods with NEGATIVE risk, means that this scenario is invalid. simply GOTO after step 4, begin next scenario.

				// NOTE: option 1, apply all the changes, then scan for any special cases and loop if there are any
				//       option 2 (active): scan for special case after every link is removed
				// neither one is guaranteed to be better or worse; it varies depending on # of links and interlinking vs # of pods in total
				int r = activepod->scan();
				switch (r) {
				case 1: // risk is negative; initial scenario was invalid... just abort this scenario
					goto LABEL_END_OF_THE_SCENARIO_LOOP;
				case 2: // risk = 0
					for (std::list<link>::iterator linkit = activepod->links.begin(); linkit != activepod->links.end(); linkit++) {
						links_to_flag_or_clear.push_front(link_with_bool(*linkit, false));
					}
					// no inc because I know there are no mines here
					copychain.podlist.erase(activepod);
					break;
				case 3: // risk = 100
						// add the remaining links in (*activepod) to links_to_flag_or_clear
						// if risk = 0, will add with 'false'... if risk = 100, will add with 'true'
					for (std::list<link>::iterator linkit = activepod->links.begin(); linkit != activepod->links.end(); linkit++) {
						struct link_with_bool t = link_with_bool(*linkit, true);
						// need to add myself to the link so i will be looked at later and determined to be disjoint
						// hopefully fixes the looping-problem!
						t.l.linked_roots.push_back(activepod->root);
						links_to_flag_or_clear.push_front(t);
					}
					// inc by the non-link cells because the links will be placed later
					//flagsthislvl += activepod->mines - activepod->links.size();
					//copychain.podlist.erase(activepod);
					break;
				case 4: // DISJOINT
					flagsthislvl += activepod->mines;
					allocsthislvl *= comb_int(activepod->mines, activepod->size());
					// allocsthislvl is only 1 if THIS pod has only 1 allocation AND all other things tried so far have only 1 allocation
					// this pod only has 1 allocation IFF mines == size
					if((mines_remaining <= RETURN_ACTUAL_ALLOCATION_IF_THIS_MANY_MINES_REMAIN) \
						&& (allocsthislvl == 1) && (activepod->mines > 0))
						cellsflaggedthislvl.insert(cellsflaggedthislvl.end(), activepod->cell_list.begin(), activepod->cell_list.end());
					copychain.podlist.erase(activepod);
					break; // not needed, but whatever
				}
			} // end of iteration on shared_roots
			if (tmp) {
				flagsthislvl++;
				if ((mines_remaining <= RETURN_ACTUAL_ALLOCATION_IF_THIS_MANY_MINES_REMAIN) && (allocsthislvl == 1))
					cellsflaggedthislvl.push_back(sharedcell);
			}
		}// end of iteration on links_to_flag_or_clear

		 // step 4: recurse! when it returns, append to resultlist its value + how many flags placed in 3a/3b/3c
		asdf = podwise_recurse(rescan_counter, copychain);
		asdf += flagsthislvl; // inc the answer for each by how many flags placed this level
		asdf *= allocsthislvl; // multiply the # allocations for this scenario into each
		if ((mines_remaining <= RETURN_ACTUAL_ALLOCATION_IF_THIS_MANY_MINES_REMAIN) && (allocsthislvl == 1))
			asdf += cellsflaggedthislvl; // add these cells into the answer for each

		retval += asdf; // append into the return list

		// if the safety valve has been activated, only try RECURSION_SAFE_WIDTH valid scenarios each level at most
		if (recursion_safety_valve && (whichscenario >= RECURSION_SAFE_WIDTH)) {
			break;
		}
		whichscenario++; // don't want this to inc on an invalid scenario
	LABEL_END_OF_THE_SCENARIO_LOOP:
		int x = 5; // need to have something here or else the label/goto gets all whiney
	}// end of iteration on the various ways to saturate frontpod


	// step 5: find the min of the mins and the max of the maxes; return them (or however else I want to combine answers)
	// perhaps I want to find the average of the mins and the average of the maxes? I'd have to change the struct to use floats then

	// version 1: max of maxes and min of mins
	// version 2: averages
	// version 3: just return retval
	if (retval.effort > RECURSION_SAFETY_LIMITER)
		recursion_safety_valve = true;

	return retval;
}



// if a goes first, return negative; if b goes first, return positive; if identical, return 0
inline int compare_two_cells(struct cell * a, struct cell * b) {
	return ((b->y * SIZEX_var) + b->x) - ((a->y * SIZEX_var) + a->x);
}

// if a goes before b, return true... needed for consistient sorting
inline bool sort_by_position(struct cell * a, struct cell * b) {
	if (compare_two_cells(a, b) < 0) { return true; } else { return false; }
}

// if a goes first, return negative; if b goes first, return positive; if identical, return 0
inline int compare_list_of_cells(std::list<struct cell *> a, std::list<struct cell *> b) {
	if (a.size() != b.size())
		return b.size() - a.size();
	// if they are the same size...
	std::list<struct cell *>::iterator ait = a.begin();
	std::list<struct cell *>::iterator bit = b.begin();
	while (ait != a.end()) {
		int c = compare_two_cells(*ait, *bit);
		if (c != 0) { return c; }
		ait++; bit++;
	}
	// i guess they're identical
	return 0;
}

// return true if the 1st arg goes before the 2nd arg, return false if 1==2 or 2 goes before 1
// intentionally ignores the shared_cell member
bool sort_scenario_blind(std::list<struct link>::iterator a, std::list<struct link>::iterator b) {
	if (compare_list_of_cells(a->linked_roots, b->linked_roots) < 0) { return true; } else { return false; }
}

// compare two scenarios, while ignoring the shared_cell arg
// if a goes first, return negative; if b goes first, return positive; if identical, return 0
inline int compare_list_of_scenarios(std::list<std::list<struct link>::iterator> a, std::list<std::list<struct link>::iterator> b) {
	if (a.size() != b.size())
		return b.size() - a.size();
	std::list<std::list<struct link>::iterator>::iterator ait = a.begin();
	std::list<std::list<struct link>::iterator>::iterator bit = b.begin();
	while (ait != a.end()) {
		int c = compare_list_of_cells((*ait)->linked_roots, (*bit)->linked_roots);
		if (c != 0) { return c; }
		ait++; bit++;
	}
	// i guess they're identical
	return 0;
}

// return true if the 1st arg goes before the 2nd arg, return false if 1==2 or 2 goes before 1
// intentionally ignores the shared_cell member
bool sort_list_of_scenarios(std::list<std::list<struct link>::iterator> a, std::list<std::list<struct link>::iterator> b) {
	if (compare_list_of_scenarios(a,b) < 0) { return true; } else { return false; }
}
bool equivalent_list_of_scenarios(std::list<std::list<struct link>::iterator> a, std::list<std::list<struct link>::iterator> b) {
	if (compare_list_of_scenarios(a, b) == 0) { return true; } else { return false; }
}


// **************************************************************************************
// multi-cell strategies

// STRATEGY: 121 cross
// looks for a very specific arrangement of visible/unknown cells; the center is probably safe
// unlike the other strategies, this isn't based in logic so much... this is just a pattern I noticed
// return # cleared if it is applied, 0 otherwise, -1 if it unexpectedly lost
// NEW VERSION: don't place any flags, only reveal the center cell (it might help)
int strat_121_cross(struct cell * center) {
	if (center->effective != 2)
		return 0;
	std::vector<struct cell *> adj = get_adjacent(center);
	if (adj.size() == 3) // must be in a corner
		return 0;

	int retme = 0;
	int r = 0;
	int s = 0;
	struct cell * aa = cellptr((center->x) + 1, center->y);
	struct cell * bb = cellptr((center->x) - 1, center->y);
	struct cell * cc = cellptr(center->x, (center->y) + 1);
	struct cell * dd = cellptr(center->x, (center->y) - 1);


	// assuming 121 in horizontal line:
	if ((aa != NULL) && (bb != NULL) && (aa->status == VISIBLE) && (bb->status == VISIBLE)
		&& (aa->effective == 1) && (bb->effective == 1)) {
		r = reveal(cc);
		s = reveal(dd);
		if ((r == -1) || (s == -1)) {
			return -1;
		} else {
			return (r + s);
		}
	}

	// assuming 121 in vertical line:
	if ((cc != NULL) && (dd != NULL) && (cc->status == VISIBLE) && (dd->status == VISIBLE)
		&& (cc->effective == 1) && (dd->effective == 1)) {
		r = reveal(aa);
		s = reveal(bb);
		if ((r == -1) || (s == -1)) {
			return -1;
		} else {
			return (r + s);
		}
	}

	return 0;
}


// STRATEGY: nonoverlap-safe
//If the adj unknown tiles of a 1/2/3 -square are a pure subset of the adj unknown tiles of another
//square with the same value, then the non-overlap section can be safely revealed!
//Technically works with 4-squares too, because the max overlap between two cells is 4, but only if
// one has 4 adj unks and the other has 5+... in that case, just use single-cell logic
//Compare against any other same-value cell in the 5x5 region minus corners
// return # of times it was applied, -1 if unexpected loss
int strat_nonoverlap_safe(struct cell * center) {
	if (center->effective > 3)
		return false;
	std::vector<struct cell *> me_unk = filter_adjacent(center, UNKNOWN);
	std::vector<struct cell *> other_unk;
	int retme = 0;
	for (int b = -2; b < 3; b++) { for (int a = -2; a < 3; a++) {
		if (((a == -2 || a == 2) && (b == -2 || b == 2)) || (a == 0 && b == 0))
			continue; // skip myself and also the corners
		struct cell * other = cellptr(center->x + a, center->y + b);
		if ((other == NULL) || (other->status != VISIBLE))
			continue;
		if (center->effective != other->effective) // the two being compared must be equal
			continue;
		other_unk = filter_adjacent(other, UNKNOWN);
		if (me_unk.size() > other_unk.size()) 
			continue; //shortcut, saves time, nov-safe only

		std::vector<std::vector<struct cell *>> nonoverlap = find_nonoverlap(me_unk, other_unk);
		// nov-safe needs to know the length of ME and the contents of OTHER
		if (nonoverlap[0].empty() && !(nonoverlap[1].empty())) {
			int retme_sub = 0;
			for (int i = 0; i < nonoverlap[1].size(); i++) {
				int r = reveal(nonoverlap[1][i]);
				if (r == -1) {
					return -1;
				}
				retme_sub += r;
			}
			retme +=retme_sub; // increment by how many were cleared
			me_unk = filter_adjacent(center, UNKNOWN); // update me_unk, then continue iterating thru the 5x5
		}
	}}
	
	return retme;
}

// STRATEGY: nonoverlap-flag
//If two cells are X and X + 1, and the unknowns around X + 1 fall inside unknowns around X except for ONE, that
//non - overlap cell must be a mine
//Expanded to the general case: if two cells are X and X+Z, and X+Z has exactly Z unique cells, then all those cells
//must be mines
//Compare against 5x5 region minus corners
//Can apply to any pair of numbers if X!=0 and Z!=0, I think?
// returns 0 if nothing happened, 1 if flags were placed, -1 if game was WON
int strat_nonoverlap_flag(struct cell * center) {
	if ((center->effective <= 1) || (center->effective == 8)) // center must be 2-7
		return false;
	//print_field(3);
	std::vector<struct cell *> me_unk = filter_adjacent(center, UNKNOWN);
	std::vector<struct cell *> other_unk;
	for (int b = -2; b < 3; b++) {
		for (int a = -2; a < 3; a++) {
			if (((a == -2 || a == 2) && (b == -2 || b == 2)) || (a == 0 && b == 0))
				continue; // skip myself and also the corners
			struct cell * other = cellptr(center->x + a, center->y+b);
			if ((other == NULL) || (other->status != VISIBLE))
				continue;
			// valid pairs are me/other: 2/1, 3/2, and 3/1; currently only know that me==2 or 3
			int z = 0;
			if ((other->effective != 0) && (other->effective < center->effective)) {
				z = center->effective - other->effective;
			} else
				continue;
			other_unk = filter_adjacent(other, UNKNOWN);

			std::vector<std::vector<struct cell *>> nonoverlap = find_nonoverlap(me_unk, other_unk);
			// nov-flag needs to know the length and contents of MY unique cells
			if (nonoverlap[0].size() == z) {
				int retval = 0;
				for (int i = 0; i < z; i++) {
					int r = set_flag(nonoverlap[0][i]);
					retval++;
					if (r == 1) {
						// already validated win in the set_flag function
						return -1;
					}
				}
				return retval;
			}
		}
	}

	return false;
}



// ********************************************************************************************

// takes argc and argv, processes input args, runs interactive prompt if appropriate
// return 0 on success, 1 if something makes me want to abort
// values are stored into global variables that replace the #define statements
// WARNING: the idiot-proofing is very weak here; guaranteed not to crash, but won't complain if entering non-numeric values
inline int parse_input_args(int argc, char *argv[]) {

// helptext:
const char helptext[1700] = "MinesweeperSolver v3.0 by Brian Henson\n\
This program is intended to generate and play a large number of Minesweeper\n\
games to collect win/loss info or whatever other data I feel like. It applies\n\
single-cell and multi-cell logical strategies as much as possible before\n\
revealing any unknown cells,of course. An extensive log is generated showing\n\
most of the stages of the solver algorithm. Each game is replayable by using the\n\
corresponding seed in the log, for closer analysis or debugging.\n\n\
*Usage/args:\n\
   -h, -?:               Print this text, then exit\n\
   -pro, -prompt:        Interactively enter various run/game settings.\n\
       Automatically chosen when no args are given.\n\
   -def, -default:       Run the program using default values\n\
*To apply settings from the command-line, use any number of these:\n\
   -num, -numgames:      How many games to play with these settings\n\
   -field:               Field size and number of mines, format= #x-#y-#mines\n\
   -findz, -findzero:    1=on, 0=off. If on, reveal zeroes during earlygame.\n\
       Not human-like but usually reaches end-game.\n\
   -smart, -smartguess:  1=on, 0=off. Replaces random guessing method with\n\
       speculative allocation of mines and risk calculation. Increases runtime\n\
       by 2-8x(avg) but increases winrate.\n\
   -seed:                0=random seed, other=specify seed. Suppresses -num \n\
       argument and plays only 1 game.\n\
   -scr, -screen:        How much printed to screen. 0=minimal clutter,\n\
       1=results for each game, 2=everything\n\n";
   


	// apply the defaults
	NUM_GAMES_var = NUM_GAMES_def;
	SIZEX_var = SIZEX_def; SIZEY_var = SIZEY_def; NUM_MINES_var = NUM_MINES_def;
	FIND_EARLY_ZEROS_var = FIND_EARLY_ZEROS_def;
	RANDOM_USE_SMART_var = RANDOM_USE_SMART_def;
	SPECIFY_SEED_var = SPECIFY_SEED_def;
	SCREEN_var = SCREEN_def;


	/*
	behavior: with no args, or with 'prompt' arg, interactively prompt the user
	help: print a block of text and exit
	also have 'default' arg to run with defaults
	if not 'default' or 'prompt' or empty, then args set/overwrite the default values... doesn't require all to be specified
	*/

	if (argc == 1) {
		// no arguments specified, do the interactive prompt thing
	LABEL_PROMPT:
		std::string bufstr;
		printf_s("Please enter settings for running the program. Defaults values are in brackets.\n");
		printf_s("Number of games: [%i]  ", NUM_GAMES_var);
		std::getline(std::cin, bufstr);
		if (bufstr.size() != 0) {
			// convert and apply
			NUM_GAMES_var = atoi(bufstr.c_str());
			// NOTE: if the input is not numeric, it simply returns 0 instead of complaining
			if (SIZEY_var < 1) {
				printf_s("ERR: #games cannot be zero or negative\n"); return 1;
			}
		}

		printf_s("Field type, format X-Y-mines: [%i-%i-%i]  ", SIZEX_var, SIZEY_var, NUM_MINES_var);
		std::getline(std::cin, bufstr);
		if (bufstr.size() != 0) {
			int f = 0;
			std::vector<int> indices;
			while (f < bufstr.size()) {
				if (bufstr[f] == '-') { indices.push_back(f); }
				f++;
			}
			if (indices.size() != 2) {
				printf_s("ERR: gamestring must have format '#-#-#', '%s' is unacceptable\n", bufstr.c_str()); return 1;
			}

			// read the values and convert them to ints, then store them
			std::string temp;
			temp = bufstr.substr(0, indices[0]);
			SIZEX_var = atoi(temp.c_str());
			temp = bufstr.substr(indices[0] + 1, indices[1] - (indices[0] + 1));
			SIZEY_var = atoi(temp.c_str());
			temp = bufstr.substr(indices[1] + 1); // from here to the end
			NUM_MINES_var = atoi(temp.c_str());

			// error checking
			if (SIZEX_var < 1) {printf_s("ERR: sizeX cannot be zero or negative\n"); return 1;}
			if (SIZEY_var < 1) {printf_s("ERR: sizeY cannot be zero or negative\n"); return 1;}
			if (NUM_MINES_var > (SIZEX_var * SIZEY_var)) {printf_s("ERR: more mines than squares in the field!\n"); return 1;}
			if (NUM_MINES_var < 1) {printf_s("ERR: #mines cannot be zero or negative\n"); return 1;}
		}


		printf_s("Always reveal zeros earlygame, 0/1: [%i]  ", FIND_EARLY_ZEROS_var);
		std::getline(std::cin, bufstr);
		if (bufstr.size() != 0) {
			// convert and apply
			FIND_EARLY_ZEROS_var = bool(atoi(bufstr.c_str()));
			// NOTE: if the input is not numeric, it simply returns 0 instead of complaining
		}

		printf_s("Use smarter guessing, 0/1: [%i]  ", RANDOM_USE_SMART_var);
		std::getline(std::cin, bufstr);
		if (bufstr.size() != 0) {
			// convert and apply
			RANDOM_USE_SMART_var = atoi(bufstr.c_str());
			// NOTE: if the input is not numeric, it simply returns 0 instead of complaining
		}

		// NOTE: decided that specifying a seed is used so rarely, it doesn't need to be prompted; -seed only is enough
		//printf_s("Specify seed to use, 0=random: [%i]  ", SPECIFY_SEED_var);
		//std::getline(std::cin, bufstr);
		//if (bufstr.size() != 0) {
		//	// convert and apply
		//	SPECIFY_SEED_var = atoi(bufstr.c_str());
		//	// NOTE: if the input is not numeric, it simply returns 0 instead of complaining
		//	//if (SPECIFY_SEED_var < 0) {
		//	//	printf_s("ERR: seed cannot be negative\n"); return 1;
		//	//}
		//	NUM_GAMES_var = 1;
		//}

		printf_s("Set printout level, 0/1/2: [%i]  ", SCREEN_var);
		std::getline(std::cin, bufstr);
		if (bufstr.size() != 0) {
			// convert and apply
			SCREEN_var = atoi(bufstr.c_str());
			// NOTE: if the input is not numeric, it simply returns 0 instead of complaining
			if (SCREEN_var > 2) { SCREEN_var = 2; }
			if (SCREEN_var < 0) { SCREEN_var = 0; }
		}
		return 0;
	}

	for (int i = 1; i < argc; i++) {

		if (!strncmp(argv[i], "-h", 2) || !strncmp(argv[i], "-?", 2)) {
			printf_s("%s", helptext);
			//system("pause"); pause outside, not here... if -h is called its definitely from command-line
			return 1; // abort execution

		} else if (!strncmp(argv[i], "-pro", 4)) {
			goto LABEL_PROMPT;

		} else if (!strncmp(argv[i], "-def", 4)) {
			printf_s("Running with default values!\n");
			return 0;

		} else if (!strncmp(argv[i], "-num", 4)) {
			if (argv[i + 1] != NULL) {
				NUM_GAMES_var = atoi(argv[i + 1]);
				// NOTE: if the argument at i+1 is not numeric, it simply returns 0 instead of complaining
				if (SPECIFY_SEED_var != 0)
					NUM_GAMES_var = 1;
				i++; continue;
			} else {
				printf_s("ERR: arg '%s' must be followed by a value, or else omitted!\n", argv[i]); return 1;
			}
		} else if (!strncmp(argv[i], "-field", 6)) {
			if (argv[i + 1] != NULL) {
				// ensure there are exactly two hyphens in the string
				int f = 0; int ct = 0;
				while (argv[i + 1][f] != '\0') {
					if (argv[i + 1][f] == '-') { ct++; }
					f++;
				}
				if (ct != 2) {
					printf_s("ERR: gamestring must have format '#-#-#', '%s' is unacceptable\n", argv[i + 1]); return 1;
				}
				// read the values and convert them to ints, then store them
				char * context = NULL;
				char * chunk = strtok_s(argv[i + 1], "-",&context);
				SIZEX_var = atoi(chunk);
				chunk = strtok_s(NULL, "-",&context);
				SIZEY_var = atoi(chunk);
				chunk = strtok_s(NULL, "-",&context);
				NUM_MINES_var = atoi(chunk);

				// error checking
				if (SIZEX_var < 1) { printf_s("ERR: sizeX cannot be zero or negative\n"); return 1; }
				if (SIZEY_var < 1) { printf_s("ERR: sizeY cannot be zero or negative\n"); return 1; }
				if (NUM_MINES_var >(SIZEX_var * SIZEY_var)) { printf_s("ERR: more mines than squares in the field!\n"); return 1; }
				if (NUM_MINES_var < 1) { printf_s("ERR: #mines cannot be zero or negative\n"); return 1; }

				i++; continue;
			} else {
				printf_s("ERR: arg '%s' must be followed by a value, or else omitted!\n", argv[i]); return 1;
			}
		} else if (!strncmp(argv[i], "-findz", 6)) {
			if (argv[i + 1] != NULL) {
				FIND_EARLY_ZEROS_var = bool(atoi(argv[i + 1]));
				// NOTE: if the argument at i+1 is not numeric, it simply returns 0 instead of complaining
				i++; continue;
			} else {
				printf_s("ERR: arg '%s' must be followed by a value, or else omitted!\n", argv[i]); return 1;
			}
		} else if (!strncmp(argv[i], "-smart", 6)) {
			if (argv[i + 1] != NULL) {
				RANDOM_USE_SMART_var = bool(atoi(argv[i + 1]));
				// NOTE: if the argument at i+1 is not numeric, it simply returns 0 instead of complaining
				i++; continue;
			} else {
				printf_s("ERR: arg '%s' must be followed by a value, or else omitted!\n", argv[i]); return 1;
			}
		} else if (!strncmp(argv[i], "-seed", 5)) {
			if (argv[i + 1] != NULL) {
				SPECIFY_SEED_var = atoi(argv[i + 1]);
				// NOTE: if the argument at i+1 is not numeric, it simply returns 0 instead of complaining
				NUM_GAMES_var = 1;
				i++; continue;
			} else {
				printf_s("ERR: arg '%s' must be followed by a value, or else omitted!\n", argv[i]); return 1;
			}
		} else if (!strncmp(argv[i], "-scr", 4)) {
			if (argv[i + 1] != NULL) {
				SCREEN_var = atoi(argv[i + 1]);
				// NOTE: if the argument at i+1 is not numeric, it simply returns 0 instead of complaining
				if (SCREEN_var > 2) { SCREEN_var = 2; }
				if (SCREEN_var < 0) { SCREEN_var = 0; }
				i++; continue;
			} else {
				printf_s("ERR: arg '%s' must be followed by a value, or else omitted!\n", argv[i]); return 1;
			}
		} else {
			printf_s("ERR: unknown argument '%s', print help with -h\n", argv[i]);
			return 1;
		}
	}

	return 0;
}

// reset_for_game: doesn't actually need to be a function, just nice to gather all the 'reset' operations together
inline void reset_for_game(std::vector<std::vector<struct cell>> * field_blank) {
	// reset stat-tracking variables
	mygamestats = game_stats();
	// reset the 'live' field
	//memcpy(field, field_blank, SIZEX_var * SIZEY_var * sizeof(struct cell)); // paste
	field = *field_blank; // dereference and paste, i hope
	
	zerolist.clear(); // reset the list
	unklist.clear();
	mines_remaining = NUM_MINES_var;
}

// generate_new_field: doesn't actually need to be a function, just nice to gather all the 'generation' operations together
inline void generate_new_field() {
	// generate the mines, populate the mine-list
	for (int i = 0; i < NUM_MINES_var; i++) {
		int x = rand() % SIZEX_var; int y = rand() % SIZEY_var;
		if (field[x][y].value == MINE) {
			i--; continue; // if already a mine, generate again
		}
		field[x][y].value = MINE;
		field[x][y].effective = MINE; // why not

		// setting up adjacency values:
		std::vector<struct cell *> adj = get_adjacent(&field[x][y]);
		for (int j = 0; j < adj.size(); j++) {

			if (adj[j]->value == MINE)
				continue; // don't touch existing mines
			adj[j]->value += 1;
			adj[j]->effective += 1;
		}

	}
	// print the fully-revealed field to screen only if SCREEN_var==2
	print_field(1, SCREEN_var);


	// iterate and populate the zero-list with all remaining zero-cell indices 
	for (int y = 0; y < SIZEY_var; y++) {
		for (int x = 0; x < SIZEX_var; x++) {
			unklist.push_back(&field[x][y]);
			if (field[x][y].value == 0)
				zerolist.push_back(&field[x][y]);
			if (field[x][y].value == 8) {
				myprintfn(2, "Found an 8 cell when generating, you must be lucky! This is incredibly rare!\n");
				myrunstats.games_with_eights++;
			}
		}
	}
	zerolist.sort(sort_by_position);
	unklist.sort(sort_by_position);
	// don't really need to sort the lists, because the find-and-remove method iterates linearly instead of using
	//   a BST, but it makes me feel better to know its sorted. and I only sort it once per game, so its fine.
}

// play one game with the field and zerolist as they currently are
// this way it's easier to lose from anywhere, without breaking out of loops
// return 1 = win, return 0 = loss
inline int play_game() {
	int r; // holds return value of any 'reveal' calls
	int numguesses = 0; // how many consecutive guesses
	char buffer[8];

	 // reveal one cell (chosen at random or guaranteed to succeed)
	if (!FIND_EARLY_ZEROS_var) {
		// reveal a random cell (game loss is possible!)
		r = reveal(rand_from_list(&unklist));
		if (r == -1) { // no need to log it, first-move loss when random-hunting is a handled situation
			return 0;
		}
		// if going to use smartguess, just pretend that the first guess was a smartguess
		if(RANDOM_USE_SMART_var) { mygamestats.trans_map = "^ "; }
		else { mygamestats.trans_map = "r "; }
	} else {
		// reveal a cell from the zerolist... game loss probably not possible, but whatever
		r = reveal(rand_from_list(&zerolist));
		if (r == -1) {
			myprintfn(2, "ERR: Unexpected loss during initial zerolist reveal, must investigate!!\n");
			return -1;
		}
		mygamestats.trans_map = "z ";
	}
	// add first entry to transition map
	//mygamestats.trans_map = "^ ";
	print_field(3, SCREEN_var); print_gamestats(SCREEN_var);

	

	// begin game-loop, continue looping until something returns
	while (1) {
		
		int action = 0; // flag indicating some action was taken this LOOP
		int numactions = 0; // how many actions have been taken during this STAGE

		// if # of mines remaining = # of unknown cells remaining, flag them and win!
		if (mines_remaining == unklist.size()) {
			numactions = mines_remaining;
			while(!unklist.empty()) {
				r = set_flag(unklist.front());
				if (r == 1) {
					// validated win! time to return!
					// add entry to transition map, use 'numactions'
					sprintf_s(buffer, "s%i ", numactions);
					mygamestats.trans_map += buffer;
					return 1;
				}
			}
		}

		////////////////////////////////////////////
		// begin single-cell logic loop
		mygamestats.gamestate = SINGLECELL;
		while(1) {
			action = 0;
			for (int y = 0; y < SIZEY_var; y++) { for (int x = 0; x < SIZEX_var; x++) { // iterate over each cell
				struct cell * me = &field[x][y];
				if (me->status != VISIBLE)
					continue; // skip

				// don't need to calculate 'effective' because it is handled every time a flag is placed
				// therefore effective values are already correct

				std::vector<struct cell *> unk = filter_adjacent(me, UNKNOWN);
				// strategy 1: if an X-adjacency cell is next to X unknowns, flag them all
				if ((me->effective != 0) && (me->effective == unk.size())) {
					// flag all unknown squares
					for (int i = 0; i < unk.size(); i++) {
						action += 1; // inc by # of cells flagged
						r = set_flag(unk[i]);
						if (r == 1) {
							// validated win! time to return!
							// add entry to transition map, use 'numactions'
							sprintf_s(buffer, "s%i ", (numactions + action));
							mygamestats.trans_map += buffer;
							return 1;
						}
					}
					unk = filter_adjacent(unk, UNKNOWN); // must update the unknown list
				}
				
				// strategy 2: if an X-adjacency cell is next to X flags, all remaining unknowns are NOT flags and can be revealed
				if (me->effective == 0) {
					// reveal all adjacent unknown squares
					for (int i = 0; i < unk.size(); i++) {
						r = reveal(unk[i]);
						if (r == -1) {
							myprintfn(2, "ERR: Unexpected loss during SC satisfied-reveal, must investigate!!\n");
							return -1;
						}
						action += r; // inc by # of cells revealed
					}
					me->status = SATISFIED;
					continue;
				}

				// additional single-cell logic? i think that's pretty much it...

			}}

			if (action != 0) {	// if something happened, then increment and loop again
				numactions += action;
				mygamestats.began_solving = true;
			} else {			// if nothing changed, then exit the loop and move on
				break;
			}

		} // end single-cell logic loop

		//sprintf_s(buffer, "s%i ", numactions);
		//mygamestats.trans_map += buffer;
		//print_field(3, SCREEN_var); print_gamestats(SCREEN_var); // post-multicell print
		if (numactions != 0) {
			numguesses = 0;
			// add entry to transition map, use 'numactions'
			sprintf_s(buffer, "s%i ", numactions);
			mygamestats.trans_map += buffer;
			print_field(3, SCREEN_var); print_gamestats(SCREEN_var); // post-singlecell print
		}

		////////////////////////////////////////////
		// begin multi-cell logic (run through once? multiple times? haven't decided)
		// (currently loops until done, or 10 loops)
		numactions = 0;
		mygamestats.gamestate = MULTICELL;
		int numloops = 0;
		while (1) {
			action = 0; 
			for (int y = 0; y < SIZEY_var; y++) { for (int x = 0; x < SIZEX_var; x++) {// iterate over each cell
				struct cell * me = &field[x][y];
				if ((me->status != VISIBLE) || (me->effective == 0))
					continue;

				// strategy 3: 121-cross
				r = strat_121_cross(me);
				if (r == 0) {// nothing happened!
					// skip
				} else if (r > 0) {// some cells were revealed!
					mygamestats.strat_121++;
					action += r; // inc by # of cells revealed
				} else if (r == -1) {// unexpected game loss, should be impossible!
					myprintfn(2, "ERR: Unexpected loss during MC 121-cross, must investigate!!\n");
					return -1;
				}

				// strategy 4: nonoverlap-flag
				r = strat_nonoverlap_flag(me);
				if (r == 0) {// nothing happened!
					// skip
				} else if (r > 0) {// some cells were flagged!
					mygamestats.strat_nov_flag++;
					action += r; // inc by # of cells flagged
				} else if (r == -1) {// game won!
					mygamestats.strat_nov_flag++;
					sprintf_s(buffer, "m%i ", (numactions + action + 1));
					mygamestats.trans_map += buffer;
					return 1;
				}

				// strategy 5: nonoverlap-safe
				r = strat_nonoverlap_safe(me);
				if (r == 0) {// nothing happened!
					// skip
				} else if (r > 0) {// some cells were revealed!
					mygamestats.strat_nov_safe++;
					action += r; // inc by # of cells flagged
				} else if (r == -1) {// unexpected game loss, should be impossible?
					myprintfn(2, "ERR: Unexpected loss during MC nonoverlap-safe, must investigate!!\n");
					return -1;
				}


			} }
			if (action != 0) {
				numactions += action;
				mygamestats.began_solving = true;
				numloops++;
				if (numloops > MULTICELL_LOOP_CUTOFF) { break; }
			} else {
				break;
			}

		}// end multi-cell logic loop
		//sprintf_s(buffer, "m%i ", numactions);
		//mygamestats.trans_map += buffer;
		//print_field(3, SCREEN_var); print_gamestats(SCREEN_var); // post-multicell print

		if (numactions != 0) {
			numguesses = 0;
			// add entry to transition map, use 'numactions'
			sprintf_s(buffer, "m%i ", numactions);
			mygamestats.trans_map += buffer;
			print_field(3, SCREEN_var); print_gamestats(SCREEN_var); // post-multicell print

			// don't do hunting, instead go back to singlecell
		} else {
			// nothing changed during multi-cell, so reveal a new cell

			// begin GUESSING phase
			mygamestats.gamestate = GUESSING;
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			int winorlose = 10;
			int method = 1; // 0=optimization, 1=guess, 2=solve
			int rxsize = 0;
			char tempchar;

			// actually guess
			if ((FIND_EARLY_ZEROS_var) && (zerolist.size()) && (mygamestats.began_solving == false)) {
				// reveal one cell from the zerolist... game loss probably not possible, but whatever
				r = reveal(rand_from_list(&zerolist));
				if (r == -1) {
					myprintfn(2, "ERR: Unexpected loss during hunting zerolist reveal, must investigate!!\n");
					winorlose = -1;
				}
				tempchar = 'z';
			} else if(!RANDOM_USE_SMART_var) {
				// random-guess
				r = reveal(rand_from_list(&unklist));
				if (r == -1) {
					winorlose = 0;
				}
				tempchar = 'r';
			} else {
				// reveal one random cell (game loss is likely!)
				// To win when random-guessing, if every guess is successful, it will reveal information that the SC/MC
				// logic will use to place the final flags. So, the only way to win is by placing flags in the right safe places
				// note: with random-guessing, old smartguess, it will only return 1 cell to clear
				// with NEW smartguess, it will usually return 1 cell to clear, but it may return several to clear or several to flag
				struct smartguess_return rx = smartguess();
				method = rx.method;
				rxsize = rx.size();
				for (int i = 0; i < rx.flagme.size(); i++) { // flag-list (uncommon but possible)
					r = set_flag(rx.flagme[i]);
					if (r == 1) {
						// fall thru, re-use the stat tracking code and return there
						winorlose = 1; break;
					}
				}
				for (int i = 0; i < rx.clearme.size(); i++) { // clear-list
					r = reveal(rx.clearme[i]);
					if (r == -1) {
						winorlose = 0; break;
					}
				}
				tempchar = '^';
			} 

			// add to stats and transition map
			int temp = 0;
			if (method == 1) {
				mygamestats.times_guessing++; numguesses++; temp = numguesses; // guessing
				if (numguesses >= 2) { // if this is the second+ hunt in a string of hunts, need to remove previous trans_map entry
					// method: (numhunts-1) to string, get length, delete that + 2 trailing chars from trans_map
					int m = (std::to_string(numguesses - 1)).size() + 2; // almost always going to be 3 or 4
					mygamestats.trans_map.erase(mygamestats.trans_map.size() - m, m);
				}
			} else {
				// this must be done after the smartguess
				numguesses = 0; temp = rxsize;
				if (method == 0) { tempchar = 'O';} // optimization stage
				else { tempchar = 'A';} // advanced solver
			}
			sprintf_s(buffer, "%c%i ", tempchar, temp);
			mygamestats.trans_map += buffer;

			if (winorlose != 10) return winorlose;

			print_field(3, SCREEN_var); print_gamestats(SCREEN_var); // post-hunt/guess print
		}

	}

	// should never hit this
	myprintfn(2, "ERR: Hit end of 'play_game' function without returning 1 or 0, must investigate!!\n");
	return -1;
}



// ************************************************************************************************
int main(int argc, char *argv[]) {
	// full-run init:
	// parse and store the input args here
	// note to self: argc has # of input args, INCLUDING PROGRAM NAME, argv is pointers to c-strings
	if (parse_input_args(argc, argv)) {
		if(argc==1) {
			system("pause"); // I know it was run from Windows Explorer so pause before it closes
		}
		return 0; // abort execution
	}


	// open log file stream
	time_t t = time(0);
	struct tm now;
	localtime_s(&now, &t);
	std::string buffer(80, '\0');
	strftime(&buffer[0], buffer.size(), "minesweeper_%m%d%H%M%S.log", &now);
	fopen_s(&logfile, buffer.c_str(), "w");
	if (!logfile) {
		printf_s("File opening failed, '%s'\n", buffer.c_str());
		system("pause");
		return 1;
	}
	myprintfn(2, "Logfile success! Created '%s'\n\n", buffer.c_str());
	myprintfn(2, "Beginning MinesweeperSolver version %s\n", VERSION_STRING_def);

	// logfile header info: mostly everything from the #defines
	myprintfn(2, "Going to play %i games, with X/Y/mines = %i/%i/%i\n", NUM_GAMES_var, SIZEX_var, SIZEY_var, NUM_MINES_var);
	if (FIND_EARLY_ZEROS_var) {
		myprintfn(2, "Using 'hunting' method = succeed early (uncover only zeroes until solving can begin)\n");
	} else {
		myprintfn(2, "Using 'hunting' method = human-like (can lose at any stage)\n");
	}

	if (RANDOM_USE_SMART_var) {
		myprintfn(2, "Using 'guessing' mode = smartguess (slower but increased winrate)\n");
	} else {
		myprintfn(2, "Using 'guessing' mode = guess randomly (lower winrate but faster)\n");
	}

	// construct a trivial random generator engine from a time-based seed:
	unsigned int seed = std::chrono::duration_cast< std::chrono::milliseconds >( 
		std::chrono::system_clock::now().time_since_epoch() 
		).count();
	std::default_random_engine generator(seed);
	std::uniform_int_distribution<int> distribution(1, INT_MAX-1);
	// invoke: distribution(generator);


	// seed random # generator
	if (SPECIFY_SEED_var == 0) {
		// create a new seed from time, log it, apply it
		myprintfn(2, "Generating new seeds\n");
	} else {
		srand(SPECIFY_SEED_var);
		NUM_GAMES_var = 1;
		myprintfn(2, "Using seed %i for game 1\n", SPECIFY_SEED_var);
	}
	fflush(logfile);


	// set up stat-tracking variables
	myrunstats = run_stats();
	// init the 'riskholder' object
	myriskholder = riskholder(SIZEX_var, SIZEY_var);

	// create 'empty' field, for pasting onto the 'live' field to reset
	struct cell asdf2 = cell(0,0); // needed for using the 'resize' command, I will overwrite the actual coords later
	// v1: call reserve -> resize... alt: call resize -> shrink_to_fit
	std::vector<struct cell> asdf = std::vector<struct cell>(); // init empty
	asdf.resize(SIZEY_var, asdf2);								// fill it with copies of asdf2
	asdf.shrink_to_fit();										// set capacity exactly where i want it, no bigger
	std::vector<std::vector<struct cell>> field_blank = std::vector<std::vector<struct cell>>();// init empty
	field_blank.resize(SIZEX_var, asdf);														// fill it with copies of asdf
	field_blank.shrink_to_fit();																// set capacity exactly where i want it, no bigger
	for (int m = 0; m < SIZEX_var; m++) {
		for (int n = 0; n < SIZEY_var; n++) {
			(field_blank[m][n]).x = m; // overwrite 0,0 with the correct coords
			(field_blank[m][n]).y = n; // overwrite 0,0 with the correct coords
		}
	}


	for (int game = 0; game < NUM_GAMES_var; game++) {

		// generate a new seed, log it, apply it... BUT only if not on the first game
		if (SPECIFY_SEED_var == 0) {
			int f = distribution(generator);
			myprintfn(SCREEN_var + 1, "Using seed %i for game %i\n", f, game + 1);
			srand(f);
		}
		// status tracker for impatient people
		printf_s("Beginning game %i of %i\n", (game + 1), NUM_GAMES_var);


		// reset various global variable things
		reset_for_game(&field_blank);

		// self-explanatory
		generate_new_field();

		// play a game, capture the return value
		int r = play_game();


		myrunstats.strat_121_total += mygamestats.strat_121;
		myrunstats.strat_nov_flag_total += mygamestats.strat_nov_flag;
		myrunstats.strat_nov_safe_total += mygamestats.strat_nov_safe;
		myrunstats.num_guesses_total += mygamestats.times_guessing;
		// increment run results depending on gamestate and gameresult
		myrunstats.games_total++;
		if (r == 0) { // game loss
			mygamestats.trans_map += "X";
			myrunstats.games_lost++;
			if (mygamestats.began_solving == false) {
				myrunstats.games_lost_beginning++;
			}
			float remaining = float(mines_remaining) / float(NUM_MINES_var);
			if (remaining > 0.85) {
				myrunstats.games_lost_earlygame++; // 0-15% completed
			} else if (remaining > 0.15) {
				myrunstats.games_lost_midgame++; // 15-85% completed
			} else {
				myrunstats.games_lost_lategame++; // 85-100% completed
			}
		} else if (r == 1) { // game win
			mygamestats.trans_map += "W";
			myrunstats.games_won++;
			if (mygamestats.times_guessing > 0) {
				myrunstats.games_won_guessing++;
			} else {
				myrunstats.games_won_noguessing++;
			}
		} else if (r == -1) {
			myrunstats.games_lost_unexpectedly++;
		}

		// print/log single-game results (also to console if #debug)
		print_field(3, SCREEN_var+1);
		print_gamestats(SCREEN_var+1);

		//printf_s("Finished game %i of %i\n", (game+1), NUM_GAMES_var);
	}

	// done with games!
	myrunstats.print_final_stats();

	fclose(logfile);
	system("pause");
    return 0;
}

