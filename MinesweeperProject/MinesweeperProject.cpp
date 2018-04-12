/* 
MinesweeperProject.cpp
Brian Henson, 4/2/2018, v3.0
This whole program (and all solver logic within it) was developed all on my own, without looking up anything
online. I'm sure someone else has done their PHD thesis on creating the 'perfect minesweeper solver' but I
didn't look at any of their work, I developed these strategies all on my own.

Contents: 
MinesweeperProject.cpp, stdafx.cpp, stdafx.h
*/

/*
v1.0: It works!!! 2 single-cell solver strategies and 3 multi-cell solver strategies, supports random-guessing or
		always-succeed guessing (to get to the endgame more reliably)
v1.1: Improved stat-tracking and printouts in too many ways to mention
v1.2: Fixed a stupid bug in nonoverlap-safe and nonoverlap-flag that prevented them from being applied at all
v1.3: Allowed for nonoverlap-safe to be applied multiple times sequentially to the same cell 
v1.4: Restructured 121-line strategy into 121-cross, because it failed a few times. Hasn't failed since the change
v1.5: Changed multi-cell logic from making only one pass to the "loop until nothing is changed" paradigm

v2.0: Added "smartguess" feature, calculates the % risk in each of the "border cells" (where visible
		adjacency cells give some information about the unknown cells' contents) and in the "interior
		cells" (where I have no information), then chooses a cell with the lowest risk.
		Sadly this algorithm is HIDEOUSLY inefficient and increases runtime by a factor of 120x (not 120%)
v2.1: MASSIVE time improvement in smartguess algorithm, by adding a number of shortcuts and limits. Time gap reduced
		to 4x - 8x
		A) shortcut: when all remaining border cells each satisfy only one adjacency cell, then don't even
		   recurse, just return the sum of their adjacency values (because at this point the order has no effect
		   on the number of mines placed). Massive improvements in early/midgame when there are "islands" of revealed
		   cells in a "sea" of unknowns, mild improvement in lategame. No loss of accuracy.
		B) width-limit: at each level (except for the initial entry-point) it only tries 2 (configurable)
		   of the unk cells tied for max size as start points for recursion, instead of all. Reduces accuracy somewhat.
		C) depth-limit: after recursing down 8 (configurable) layers of recursion, AKA placing 8 mines, it
		   stops branching at all and sets the width-limit to 1. Reduces accuracy somewhat.
v2.2: Changed recursive algorithm finding minimum allocation of border mines to instead find
		the MAXIMUM allocation when starting at the most "crowded" cells; a sorta compromise/split,
		finding the maximum of the minimum allocation or something. I don't understand why it works
		better, but it more closely approximates the true number of mines in the border cells. Slight success
		improvement, I forget the number :(
		(Old avg deviation approx -0.4, new avg deviation approx -0.29)
		Also prints the avg deviation number, because I can.
v2.3: Changed border-cell risk calculation to use average of all possibilities instead of max, slight
		success improvement (30.8% -> 31.7% over 3k games) 

v3.0: Changed some ways variables/data are stored (unimportant)
		Compiled it in "release" mode, preparing for post on Facebook
		   MAJOR NOTE: when compiled in "release" mode it goes approx. 100x faster!!!!!!!!!!!!!!!!!!! Average game
		   time is down below 10ms!!!!!!!! When using DEBUG=0, average game time down below 2ms!!!!!!!
		Increased resolution of 'elapsed time' to account for going so much faster
		Added minimum delay of 1ms to each game, just in case, to guarantee each game has a unique seed
		Added command-line args, interactive prompting for settings, #defines now only set the default values
		Also removed some of what's printed to screen on DEBUG=1, now its only end-game status and summary
		The field is printed to log at each stage only when DEBUG=1/2, but transition map & stats are always printed to log
*/

//TODO: change 'smartguess' method to use 'pods'




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
		times_hunting = 0;
		times_guessing = 0;
		gamestate = GUESSING; // first stage is a blind guess
		began_guessing = false;
		began_solving = false;
	}
	int strat_121, strat_nov_safe, strat_nov_flag; // number of times each MC strategy was used
	int times_hunting, times_guessing; // number of times it needed to do hunting or guessing
	//records the phase transition history of solving the game, and the # of operations done in each phase
	// ^=hunting, G=guessing, s=single-cell, m=multi-cell
	std::string trans_map;
	game_state gamestate; // where in the solver algorithm I am, to track where it was when it ended
	bool began_solving; // did I begin solving things, or did I lose before anything could be done?
	bool began_guessing; // how to tell if the game was deterministic or not
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
		games_lost_hunting = 0;
		games_lost_guessing = 0;
		games_lost_unexpectedly = 0;
		strat_121_total = 0;
		strat_nov_safe_total = 0;
		strat_nov_flag_total = 0;
		smartguess_attempts = 0;
		smarguess_maxret_diff = 0;
		games_with_eights = 0;
	}

	long start;
	int games_total;			// total games played to conclusion (not really needed but w/e)

	int games_won;				// total games won
	int games_won_noguessing;	// games won without "end-game guessing"
	int games_won_guessing;		// games won by "end-game guessing"

	int games_lost;				// total games lost
	int games_lost_beginning;	// games lost before any logic could be applied (0 when hunting correctly)
	int games_lost_hunting;		// games lost in random hunting (0 when hunting correctly)
	int games_lost_guessing;	// games lost in end-game guessing
	int games_lost_unexpectedly;// losses from other situations (should be 0)

	int strat_121_total;
	int strat_nov_safe_total;
	int strat_nov_flag_total;
	int smartguess_attempts;
	int smarguess_maxret_diff;
	int games_with_eights;
};


// TODO: can consolidate risk_unk and risk_adj into one struct; but either the 'risk' OR the 'eff' values would be unused at any time
// upside: simpler, smaller recursion code... downside: less memory-efficient
struct risk_unk {
	risk_unk() {}
	risk_unk(struct cell * itsme) {
		me = itsme;
		//risk = 0.;
	}
	struct cell * me;
	std::list<float> risk;
	std::list<struct cell *> Nlist; // list of neighboring visible adjacency cells
};

struct risk_adj {
	risk_adj() {}
	risk_adj(struct cell * itsme) {
		me = itsme;
		eff = itsme->effective;
	}
	struct cell * me;
	short eff;
	std::list<struct cell *> Nlist; // list of neighboring risk_unk structs
};


// object returned by the recursive layers; could easily add other returnable info (like the exact allocation found)
struct recursion_return {
	recursion_return() {}
	recursion_return(int max) {
		//minret = min;
		maxret = max;
	}
	//int minret;
	int maxret;
};







// just because I'm sick of calling printf_s and fprintf_s with the same args
// function-like macro that accepts a variable number/type of args, very nifty
// always prints to the logfile; printing to screen depends on #DEBUG_var
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
#define NUM_GAMES_def				1000
#define SIZEX_def					30
#define SIZEY_def					16
#define NUM_MINES_def				90
#define	HUNT_FIND_ZEROS_def			false
#define RANDOM_USE_SMART_def		true
// if SPECIFY_SEED_var = 0, will generate a new seed from current time
#define SPECIFY_SEED_def			0
// controls what gets printed to the console
// 0: prints almost nothing to screen, 1: prints game-end to screen, 2: prints everything to screen
#define DEBUG_def					1

#define RECURSIVE_EFFORT			8	// might increase smartguess accuracy very slightly, but will DRASTICALLY increase time
#define RECURSIVE_WIDTH_DONT_CHANGE 2	// might increase smartguess accuracy very slightly, but will DRASTICALLY increase time



// global vars
int NUM_GAMES_var = 0;
int SIZEX_var = 0;
int SIZEY_var = 0;
int NUM_MINES_var = 0;
bool HUNT_FIND_ZEROS_var = false;
bool RANDOM_USE_SMART_var = false;
int SPECIFY_SEED_var = 0;
int DEBUG_var = 0;


int mines_remain = 0;
std::list<struct cell *> zerolist; // zero-list
std::list<struct cell *> unklist; // unknown-list, hopefully allows for faster "better rand"
std::vector<std::vector<struct cell>> field; // the actual playing field; overwritten for each game
FILE * logfile; // file stream to the logfile (globally accessible)
struct game_stats mygamestats; // stat-tracking variables
struct run_stats myrunstats; // stat-tracking variables



/*
NOTE: even with "random" hunting, when there are no more zeroes left, it counts as "end-game guessing"
*/


// function declarations
// big/fundamental actions that deserve to be functions
inline struct cell * cellptr(int x, int y);
std::vector<struct cell *> get_adjacent(struct cell * me);
int reveal(struct cell * me);
int set_flag(struct cell * me);
std::vector<struct cell *> filter_adjacent(struct cell * me, cell_state target);
std::vector<struct cell *> filter_adjacent(std::vector<struct cell *> adj, cell_state target);
std::vector<std::vector<struct cell *>> find_nonoverlap(std::vector<struct cell *> me_unk, std::vector<struct cell *> other_unk);
struct cell * rand_from_list(std::list<struct cell *> * fromme);
struct cell * rand_unk();
struct recursion_return recursive_allocate_border_mines(std::list<struct risk_adj> adj_list, std::list<struct risk_unk> unk_list, struct cell * target, int current_depth);
// small but essential utility functions
inline std::list<struct risk_unk>::iterator lookup_adj2unk(struct cell * target, std::list<struct risk_unk> * unklist);
inline std::list<struct risk_adj>::iterator lookup_unk2adj(struct cell * target, std::list<struct risk_adj> * adjlist);
bool compare_by_position(struct cell * a, struct cell * b);
bool compare_2(struct risk_unk a, struct risk_unk b);
bool equivalent_1(struct risk_unk a, struct risk_unk b);
bool compare_3(struct risk_unk * a, struct risk_unk * b);
bool equivalent_2(struct risk_unk * a, struct risk_unk * b);
// print/display functions
void print_field(int mode, int screen);
void print_gamestats(int screen);
// mult-cell strategies
int strat_121_cross(struct cell * center);
int strat_nonoverlap_safe(struct cell * center);
int strat_nonoverlap_flag(struct cell * center);
// major structural functions (just for encapsulation)
inline int parse_input_args(int margc, char *margv[]);
inline void reset_for_game(std::vector<std::vector<struct cell>> * field_blank);
inline void generate_new_field();
inline int play_game();





// function definitions

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


// reveal: recursive function, returns -1 if loss or the # of non-zero cells uncovered otherwise
// if it's a zero, remove it from the zero-list and recurse
// doesn't complain if you give it a null pointer
int reveal(struct cell * revealme) {
	if (revealme == NULL) return 0;
	if (revealme->status != UNKNOWN)
		return 0; // if it is flagged, satisfied, or already visible, then do nothing. will happen often.
	revealme->status = VISIBLE;
	unklist.remove(revealme);
	if (revealme->value == MINE) {
		// lose the game, handled wherever calls here
		return -1;
	} else if(revealme->effective == 0) {
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
		return retme;
	} else {
		// if its an adjacency number, return 1
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
		int x = 5 + 2; // always have debug breakpoint here
	}
	assert(flagme->status == UNKNOWN); // if flagged/visible/satisfied, then why was this called? need to do a replay

	flagme->status = FLAGGED;
	unklist.remove(flagme);

	// check if it flagged a non-mine square, and if so, why?
	// use assert() statement to dump a stack trace? doesn't dump a trace :(
	if (flagme->value != MINE) {
		int x = 5 + 2; // always have debug breakpoint here
	}
	assert(flagme->value == MINE);

	// reduce "effective" values of everything around it, whether its visible or not
	std::vector<struct cell *> adj = get_adjacent(flagme);
	for (int i = 0; i < adj.size(); i++) {
		if(adj[i]->value != MINE)
			adj[i]->effective -= 1;
	}

	// decrement the remaining mines
	mines_remain -= 1;
	if (mines_remain == 0) {
		//try to validate the win: every mine is flagged, and every not-mine is not-flagged
		for (int y = 0; y < SIZEY_var; y++) {for (int x = 0; x < SIZEX_var; x++) { // iterate over each cell
			struct cell * v = &field[x][y];
			if (v->value == MINE) {
				if (v->status != FLAGGED) {
					int x = 5 + 2; // always have debug breakpoint here
				}
				// assert that every mine is flagged
				assert(v->status == FLAGGED);
			}
			if (v->value != MINE) {
				if (v->status == FLAGGED) {
					int x = 5 + 2; // always have debug breakpoint here
				}
				// assert that every not-mine is not-flagged
				assert(v->status != FLAGGED);
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


// is_subset: compare two cells' adj unknown vectors, and return everything adj to ME but not adj to OTHER
// the shortcut comparison can be done outside, comparing the sizes of the two to eliminate half of comparisons
//if (me_unk.size() > other_unk.size()) {return false;}
std::vector<std::vector<struct cell *>> find_nonoverlap(std::vector<struct cell *> me_unk, std::vector<struct cell *> other_unk) {
	for (int i = 0; i != me_unk.size(); i++) { // for each cell in me_unk...
		for (int j = 0; j != other_unk.size(); j++) { // ... compare against each cell in other_unk...
			if (me_unk[i] == other_unk[j]) {// ... until there is a match!
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
	return retme;
}


// print: either 1) fully-revealed field, 2) in-progress field as seen by human, 3) in-progress field showing 'effective' values
// borders made with +, zeros: blank, adjacency (or effective): number, unknown: -, flag or mine: *
// when DEBUG is low enough, skipped entirely (doesn't even print to log)
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
	myprintfn(screen, "Cells hunted: %i, cells guessed: %i\n",
		mygamestats.times_hunting, mygamestats.times_guessing);
	myprintfn(screen, "Flags placed: %i / %i\n\n\n", NUM_MINES_var - mines_remain, NUM_MINES_var);
	fflush(logfile);
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



// EITHER return an unknown cell (for revealing) chosen by unintelligent pure random selection...
// ...OR return an unknown cell (for revealing) chosen by laboriously determining the % risk of each unknown cell
struct cell * rand_unk() {
	if (unklist.empty())
		return NULL;
	if (RANDOM_USE_SMART_var) {
		/*
		have access to accurate unklist, might save some time?
		probably need to iterate over the field and build an adjacency-list anyway tho
		need to have some special struct that point to a cell, but also has the % risk and the adj cells it would satisfy???

		1. sort unknown cells into "border" and "interior"
		2. calculating risk % for each border cell is easy, simply the max of the risk from each adj cell
			TODO: consider instead using average of risk from each adj cell?
			iterate over each adj cell, calculate risk, apply to all surrounding unknowns
		3. to find risk % for interior cells, need to know number of mines not "accounted for" in the border
		4. must calculate (minimal) and (crowded) allocations of mines in the border cells
			NEW: better results to sorta split the difference and take the maximum result of the (minimal) allocation
			interior risk = (total mines - mines border cells) / (# interior)
			TODO: if there is only one allocation of mines found, apply it immediately?
			would probably be very slow
		*/
		std::list<struct risk_adj> adj_list;
		std::list<struct risk_unk> border_list;
		std::list<struct cell *> interior_list = unklist;

		// iterate, get visible + unclassified unknown, visible->unk means remove from general, add to border
		for (int y = 0; y < SIZEY_var; y++) {for (int x = 0; x < SIZEX_var; x++) { // iterate over each cell
			if (field[x][y].status == VISIBLE) {
				adj_list.push_back(risk_adj(&field[x][y]));
				std::vector<struct cell *> unk = filter_adjacent(&field[x][y], UNKNOWN); // find all the unknown around this adj
				for (int i = 0; i < unk.size(); i++) {
					interior_list.remove(unk[i]); // remove from interior_list...
					border_list.push_front(risk_unk(unk[i])); // ...and add to border_list! will create duplicates, oh well
				}
			}
		}}
		border_list.sort(compare_2); // need to sort by the x/y position
		border_list.unique(equivalent_1); // remove duplicates
		
		// don't need to store the interior_list risk data, it's all the same... just say (one of these chosen at random) if its best
		// now adj_list, border_list, and interior_list have the right contents... but adj_ and border_ still need cross-linked
		// border_list
		for (std::list<struct risk_unk>::iterator itunk = border_list.begin(); itunk != border_list.end(); itunk++) {
			std::vector<struct cell *> temp = filter_adjacent(itunk->me, VISIBLE);
			temp.shrink_to_fit();
			itunk->Nlist = std::list<struct cell *>(temp.begin(), temp.end());
		}
		// adj_list
		for (std::list<struct risk_adj>::iterator itadj = adj_list.begin(); itadj != adj_list.end(); itadj++) {
			// for each risk_adj 'itadj' in adj_list...
			std::vector<struct cell *> Nunk = filter_adjacent(itadj->me, UNKNOWN);
			Nunk.shrink_to_fit();
			itadj->Nlist = std::list<struct cell *>(Nunk.begin(), Nunk.end());
		}

		// now adj_list and border_list are cross-linked and everything is ready to recurse
		// Nlist in adj_list point at the cells that match the ones in border_list
		// Nlist in border_list point at the cells that match the ones in adj_list
		// beginning the recursion about halfway thru a recursive function call (very ugly but oh well)

		// recursion strategy: place a hypothetical mine at the border cell that would satisfy the most adjacency cells, then repeat
		//                     starting at a most-crowded cell is referred to as 'underestimate'
		//                     trying multiple different "paths" if multiple border cells are tied for size
		// note: if the start point is the border cell that would satisfy the FEWEST adjacency cells, then a 'overestimate' can be found...
		//       ... but it is much easier to optimize the 'underestimate' paths for time
		// note: turns out that taking the MAXIMUM of the 'underestimate' strategy produces a result on average slightly closer to the hidden 
		//       true number of mines in the border cells than taking the MINIMUM of the 'underestimate' strategy
		// i'm curious how the MINIMUM of the 'overestimate' strategy would compare but don't want to bother writing it

		float interior_risk = 100;
		if (!interior_list.empty()) {
			std::list<struct risk_unk *> maxlist;
			int maxsize = 0;
			// for each risk_adj in adj_list, look through the Nlist at all risk_unk, and look at the size of THEIR Nlist
			// collect everything tied for max size
			for (std::list<struct risk_adj>::iterator itadj = adj_list.begin(); itadj != adj_list.end(); itadj++) {
				// for each risk_adj 'itadj' in adj_list...
				for (std::list<struct cell *>::iterator whichcell = itadj->Nlist.begin(); whichcell != itadj->Nlist.end(); whichcell++) {
					// for each cell* 'whichcell' in Nlist of itadj...
					// ... turn the cell* into a risk_unk 'p' in unk_list
					std::list<struct risk_unk>::iterator p = lookup_adj2unk(*whichcell, &border_list);
					int j = p->Nlist.size();
					if (j == maxsize) {
						maxlist.push_back(&(*p));
					} else if (j > maxsize) {
						maxsize = j;
						maxlist.clear();
						maxlist.push_back(&(*p));
					}
				}
			}
			maxlist.sort(compare_3); maxlist.unique(equivalent_2);

			// for each risk_unk* in maxlist, recurse with the current adj_list and unk_list using the cell* from the selected risk_unk as the target
			//int minret = 100;
			int maxret = 0;
			if (maxsize == 1) {
				// if maxsze==1, then each unk it might use as a target only satisfies one adj cell. So, the order doesn't matter at all!
				// minret = the sum of the 'eff' values for all remaining risk_adj
				//minret = 0;
				for (std::list<struct risk_adj>::iterator itadj = adj_list.begin(); itadj != adj_list.end(); itadj++) {
					//minret += itadj->eff; 
					maxret += itadj->eff;
				}
			} else {
				for (std::list<struct risk_unk *>::iterator itmax = maxlist.begin(); itmax != maxlist.end(); itmax++) {
					struct recursion_return r = recursive_allocate_border_mines(adj_list, border_list, (*itmax)->me, 1);
					//if (r.minret < minret) { minret = r.minret; }
					if (r.maxret > maxret) { maxret = r.maxret; }
					// don't limit the recursive width here... at the highest level (this) i want to try all start points
				}
			}

			// just some sanity-checking stuff
			//if (border_list.empty()) { minret = 0; } // if border_list is empty the loops won't run, so this is a adequate solution
			//if (minret > mines_remain) { minret = mines_remain; }
			if (maxret > mines_remain) { maxret = mines_remain; }


			////////////////////////////////////////////////////////////////////////
			// DEBUG STUFF
			// question: how accurate is the count of the border mines? are the limits making the algorithm get a wrong answer?
			// answer: compare 'minret' with a total omniscient count of the mines in the border cells
			int bordermines = 0;
			for (std::list<struct risk_unk>::iterator itunk = border_list.begin(); itunk != border_list.end(); itunk++) {
				if (itunk->me->value == MINE)
					bordermines++;
			}

			//myprintfn(2, "DEBUG: in smart-guess function, minret/maxret/bordermines = %i / %i / %i\n", minret, maxret, bordermines);
			myrunstats.smartguess_attempts++;
			//myrunstats.smarguess_minret_diff += (minret - bordermines); // accumulating a negative number
			myrunstats.smarguess_maxret_diff += (maxret - bordermines); // accumulating a less-negative number
			////////////////////////////////////////////////////////////////////////


			// calculate the risk of a mine in any 'interior' cell (they're all the same)
			interior_risk = float(mines_remain - maxret) / float(interior_list.size());

		}

		// now the interior_risk has been calculated to some accuracy

		// calculate the risk of a mine in any border cell
		for (std::list<struct risk_adj>::iterator itadj = adj_list.begin(); itadj != adj_list.end(); itadj++) {
			// for each risk_adj 'itadj' in adj_list...
			assert(itadj->eff == itadj->me->effective); // for testing only, so I'm convinced
			float risk = float(itadj->eff) / float(itadj->Nlist.size());
			for (std::list<struct cell *>::iterator itcell = itadj->Nlist.begin(); itcell != itadj->Nlist.end(); itcell++) {
				// for each cell* 'itcell' in Nlist of itadj...
				std::list<struct risk_unk>::iterator t = lookup_adj2unk(*itcell, &border_list);
				t->risk.push_back(risk); // accumulate all the risks from all adjacent cells
			}
		}

		for (std::list<struct risk_unk>::iterator itunk = border_list.begin(); itunk != border_list.end(); itunk++) {
			// for each risk_unk...
			float avg_risk = 0;
			for (std::list<float>::iterator itf = itunk->risk.begin(); itf != itunk->risk.end(); itf++) {
				// accumulate the risks from various sources into a single sum
				avg_risk += *itf;
			}
			// divide by size to turn it into an average
			avg_risk = avg_risk / itunk->risk.size();
			itunk->risk.clear();
			// now the only item in the 'risk list' is the averaged value
			itunk->risk.push_back(avg_risk);
		}


		// get everything in border_list with (or tied for) lowest risk
		std::list<struct risk_unk *> minlist;
		float minrisk = 100;
		for (std::list<struct risk_unk>::iterator itunk = border_list.begin(); itunk != border_list.end(); itunk++) {
			//float j = (*itunk).risk;
			float j = (*itunk).risk.front();
			if (j == minrisk) {
				minlist.push_back(&(*itunk));
			} else if (j < minrisk) {
				minrisk = j;
				minlist.clear();
				minlist.push_back(&(*itunk));
			}
		}

		// now everything from the border with the risk = minrisk is in minlist, and the interior_risk is known
		if ((interior_risk < minrisk) && (!interior_list.empty())){
			return rand_from_list(&interior_list);
		} else {
			int f = rand() % minlist.size();
			std::list<struct risk_unk *>::iterator itmin = minlist.begin();
			for (int i = 0; i < f; i++) { itmin++; }
			return (*itmin)->me;
		}



	} else {
		return rand_from_list(&unklist);
	}
}


// recursive
struct recursion_return recursive_allocate_border_mines(std::list<struct risk_adj> adj_list, std::list<struct risk_unk> unk_list, struct cell * target, int current_depth) {
	// place a "flag" on the target:
	std::list<struct risk_unk>::iterator t = lookup_adj2unk(target, &unk_list);
	std::vector<struct cell *> holder;
	for (std::list<struct cell *>::iterator i = t->Nlist.begin(); i != t->Nlist.end(); i++) {
		// for each adj cell 'i' in Nlist of risk_unk 't' (from target)
		// match an adj_list entry to the cell, guaranteed to exist (i hope)
		std::list<struct risk_adj>::iterator itadj = lookup_unk2adj(*i, &adj_list);
		// decrement adj #
		itadj->eff -= 1;
		// if zero, then remove it
		if (itadj->eff == 0) {
			holder.push_back(itadj->me); // wont have any duplicates
			adj_list.erase(itadj); // delete by iterator
			// if list is empty, then return 1 (should never happen, other return section should activate instead)
			if (adj_list.size() == 0)
				return recursion_return(1);
		} else {
			// if not removed, then remove 'target' from its Nlist
			itadj->Nlist.remove(target); // delete by value
		}
	}
	
	// now i need to remove from Nlist in risk_unk in unk_list any references to the risk_adj cells that were removed
	for (std::list<struct risk_unk>::iterator itunk = unk_list.begin(); itunk != unk_list.end(); itunk++) {
		// for every risk_unk 'itunk' in unk_list...
		for (int i = 0; i < holder.size(); i++) {
			// ... for every cell that was removed from adj_list...
			//... remove any references to it in Nlist
			itunk->Nlist.remove(holder[i]);
		}
	}

	std::list<struct risk_unk *> maxlist;
	int maxsize = 0;
	// for each risk_adj in adj_list, look through the Nlist at all risk_unk, and look at the size of THEIR Nlist
	// collect everything tied for max size
	// AKA which unknown cell will satisfy the most visible adjacency cells, or tied for most
	for (std::list<struct risk_adj>::iterator itadj = adj_list.begin(); itadj != adj_list.end(); itadj++) {
		// for each risk_adj 'itadj' in adj_list...
		for (std::list<struct cell *>::iterator whichcell = itadj->Nlist.begin(); whichcell != itadj->Nlist.end(); whichcell++) {
			// for each cell* 'whichcell' in Nlist of itadj...
			// ... turn the cell* into a risk_unk 'p' in unk_list
			std::list<struct risk_unk>::iterator p = lookup_adj2unk(*whichcell, &unk_list);
			int j = p->Nlist.size();
			if (j == maxsize) {
				maxlist.push_back(&(*p));
			} else if (j > maxsize) {
				maxsize = j;
				maxlist.clear();
				maxlist.push_back(&(*p));
			}
		}
	}
	maxlist.sort(compare_3); maxlist.unique(equivalent_2);

	// for each risk_unk* in maxlist, recurse with the current adj_list and unk_list using the cell* from the selected risk_unk as the target
	//int minret = 100;
	int maxret = 0;
	int how_wide = 0;
	if (maxsize == 1) {
		// if maxsze==1, then each unk it might use as a target only satisfies one adj cell. So, the order doesn't matter at all!
		// minret = the sum of the 'eff' values for all remaining risk_adj
		//minret = 0;
		for (std::list<struct risk_adj>::iterator itadj = adj_list.begin(); itadj != adj_list.end(); itadj++) {
			//minret += itadj->eff;
			maxret += itadj->eff;
		}
	} else {
		for (std::list<struct risk_unk *>::iterator itmax = maxlist.begin(); itmax != maxlist.end(); itmax++) {
			struct recursion_return r = recursive_allocate_border_mines(adj_list, unk_list, (*itmax)->me, current_depth + 1);
			//if (r.minret < minret) { minret = r.minret; }
			if (r.maxret > maxret) { maxret = r.maxret; }
			if (current_depth >= RECURSIVE_EFFORT) // kludge: after going this deep, stop trying the possibilities at all
				break;
			how_wide++;
			if (how_wide >= RECURSIVE_WIDTH_DONT_CHANGE) // kludge: only try 2 of the paths starting from here, instead of all of them
				break;
		}
	}

	// return the lowest value of any value a recursion returns, +1 (for me) because it should return the depth it went to
	return recursion_return(maxret+1);
}


// when given a pointer to a cell, and a list of risk_unk, find a risk_unk corresponding to the target
inline std::list<struct risk_unk>::iterator lookup_adj2unk(struct cell * target, std::list<struct risk_unk> * unklist) {
	std::list<struct risk_unk>::iterator it;
	for (it = unklist->begin(); it != unklist->end(); it++) {
		if (it->me == target)
			return it;
	}
	return unklist->end(); // should never happen
}

// when given a pointer to a cell, and a list of risk_adj, find a risk_adj corresponding to the target
inline std::list<struct risk_adj>::iterator lookup_unk2adj(struct cell * target, std::list<struct risk_adj> * adjlist) {
	std::list<struct risk_adj>::iterator itadj;
	for (itadj = adjlist->begin(); itadj != adjlist->end(); itadj++) {
		if (itadj->me == target)
			return itadj;
	}
	return adjlist->end(); // should never happen
}

// if a goes before b, return true... needed for consistient sorting
bool compare_by_position(struct cell * a, struct cell * b) {
	return (((a->y * SIZEX_var) + a->x) < ((b->y * SIZEX_var) + b->x));
}
// sort by the .me element, needed to get sorted so duplicates can be removed
bool compare_2(struct risk_unk a, struct risk_unk b) {
	return compare_by_position(a.me, b.me);
}

bool equivalent_1(struct risk_unk a, struct risk_unk b) {
	return (((a.me->y * SIZEX_var) + a.me->x) == ((b.me->y * SIZEX_var) + b.me->x));
}

bool compare_3(struct risk_unk * a, struct risk_unk * b) {
	return compare_by_position(a->me, b->me);
}

bool equivalent_2(struct risk_unk * a, struct risk_unk * b) {
	return (((a->me->y * SIZEX_var) + a->me->x) == ((b->me->y * SIZEX_var) + b->me->x));
}


// **************************************************************************************
// multi-cell strategies

// STRATEGY: 121 cross
// looks for a very specific arrangement of visible/unknown cells; the center is probably safe
// unlike the other strategies, this isn't based in logic so much... this is just a pattern I noticed
// return 1 if it is applied, 0 otherwise, -1 if it unexpectedly lost
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
			myprintfn(2, "Unexpected loss during MC 121-cross, must investigate!!\n");
			return -1;
		} else {
			return bool(r + s); // reduce to 1 or 0
		}
	}

	// assuming 121 in vertical line:
	if ((cc != NULL) && (dd != NULL) && (cc->status == VISIBLE) && (dd->status == VISIBLE)
		&& (cc->effective == 1) && (dd->effective == 1)) {
		r = reveal(aa);
		s = reveal(bb);
		if ((r == -1) || (s == -1)) {
			myprintfn(2, "Unexpected loss during MC 121-cross, must investigate!!\n");
			return -1;
		} else {
			return bool(r + s); // reduce to 1 or 0
		}
	}

	return 0;
}


// STRATEGY: nonoverlap-safe
//If the adj unknown tiles of a 1/2 -square are a pure subset of the adj unknown tiles of another
//square with the same value, then the non - overlap section can be safely revealed!
//Compare against any other same - value cell in the 5x5 region minus corners
// return # of times it was applied, -1 if unexpected loss
int strat_nonoverlap_safe(struct cell * center) {
	if (!((center->effective == 1) || (center->effective == 2) || (center->effective == 3)))
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

		// TODO: could probably increase efficiency by splicing/merging nov-safe and nov-flag... would be difficult
		// to combine them tho. might make stat-tracking difficult, might require updating the unknown-lists after
		// the first one successfully operates, more hard stuff...

		std::vector<std::vector<struct cell *>> nonoverlap = find_nonoverlap(me_unk, other_unk);
		// nov-safe needs to know the length of ME and the contents of OTHER
		if (nonoverlap[0].empty() && !(nonoverlap[1].empty())) {
			int retme_sub = 0;
			for (int i = 0; i < nonoverlap[1].size(); i++) {
				int r = reveal(nonoverlap[1][i]);
				if (r == -1) {
					myprintfn(2, "Unexpected loss during MC nonoverlap-safe, must investigate!!\n");
					return -1;
				}
				retme_sub += r;
			}
			retme += bool(retme_sub); // increment by 1 or 0, regardless of how many were cleared
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
// returns 0 if nothing happened, 1 if flags were placed, 100 if game was won
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
				for (int i = 0; i < z; i++) {
					int r = set_flag(nonoverlap[0][i]);
					if (r == 1) {
						// already validated win in the set_flag function
						return 100;
					}
				}
				return 1;
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
   -findz, -findzero:    1=on, 0=off. If on, always reveal zero if any remain.\n\
       Not human-like but always reaches end-game.\n\
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
	HUNT_FIND_ZEROS_var = HUNT_FIND_ZEROS_def;
	RANDOM_USE_SMART_var = RANDOM_USE_SMART_def;
	SPECIFY_SEED_var = SPECIFY_SEED_def;
	DEBUG_var = DEBUG_def;


	/*
	behavior: with no args, or with 'prompt' arg, interactively prompt the user
	help: print a block of text and exit
	also have 'default' arg to run with defaults
	if not 'default' or 'prompt' or empty, then args set/overwrite the default values... doesn't require all to be specified
	*/

	if (argc == 1) {
		// no arguments specified, do the interactive prompt thing
	label_prompt:
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

		//printf_s("Field size X: [%i]  ", SIZEX_var);
		//std::getline(std::cin, bufstr);
		//if (bufstr.size() != 0) {
		//	// convert and apply
		//	SIZEX_var = atoi(bufstr.c_str());
		//	// NOTE: if the input is not numeric, it simply returns 0 instead of complaining
		//	if (SIZEX_var < 1) {
		//		printf_s("ERR: sizeX cannot be zero or negative\n"); return 1;
		//	}
		//}
		//printf_s("Field size Y: [%i]  ", SIZEY_var);
		//std::getline(std::cin, bufstr);
		//if (bufstr.size() != 0) {
		//	// convert and apply
		//	SIZEY_var = atoi(bufstr.c_str());
		//	// NOTE: if the input is not numeric, it simply returns 0 instead of complaining
		//	if (SIZEY_var < 1) {
		//		printf_s("ERR: sizeY cannot be zero or negative\n"); return 1;
		//	}
		//}
		//printf_s("Number mines in field: [%i]  ", NUM_MINES_var);
		//std::getline(std::cin, bufstr);
		//if (bufstr.size() != 0) {
		//	// convert and apply
		//	NUM_MINES_var = atoi(bufstr.c_str());
		//	// NOTE: if the input is not numeric, it simply returns 0 instead of complaining
		//	if (NUM_MINES_var > (SIZEX_var * SIZEY_var)) {
		//		printf_s("ERR: #mines > sizeX * sizeY\n"); return 1;
		//	}
		//	if (NUM_MINES_var < 1) {
		//		printf_s("ERR: #mines cannot be zero or negative\n"); return 1;
		//	}
		//}

		printf_s("Always reveal zeros, 0/1: [%i]  ", HUNT_FIND_ZEROS_var);
		std::getline(std::cin, bufstr);
		if (bufstr.size() != 0) {
			// convert and apply
			HUNT_FIND_ZEROS_var = bool(atoi(bufstr.c_str()));
			// NOTE: if the input is not numeric, it simply returns 0 instead of complaining
		}

		printf_s("Use smarter guessing, 0/1: [%i]  ", RANDOM_USE_SMART_var);
		std::getline(std::cin, bufstr);
		if (bufstr.size() != 0) {
			// convert and apply
			RANDOM_USE_SMART_var = bool(atoi(bufstr.c_str()));
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

		printf_s("Set printout level, 0/1/2: [%i]  ", DEBUG_var);
		std::getline(std::cin, bufstr);
		if (bufstr.size() != 0) {
			// convert and apply
			DEBUG_var = atoi(bufstr.c_str());
			// NOTE: if the input is not numeric, it simply returns 0 instead of complaining
			if (DEBUG_var > 2) { DEBUG_var = 2; }
			if (DEBUG_var < 0) { DEBUG_var = 0; }
		}
		return 0;
	}

	for (int i = 1; i < argc; i++) {

		//printf_s("%s\n", argv[i]); // debug

		if (!strncmp(argv[i], "-h", 2) || !strncmp(argv[i], "-?", 2)) {
			printf_s("%s", helptext);
			system("pause");
			return 1; // abort execution

		} else if (!strncmp(argv[i], "-pro", 4)) {
			goto label_prompt;

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
				HUNT_FIND_ZEROS_var = bool(atoi(argv[i + 1]));
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
				DEBUG_var = atoi(argv[i + 1]);
				// NOTE: if the argument at i+1 is not numeric, it simply returns 0 instead of complaining
				if (DEBUG_var > 2) { DEBUG_var = 2; }
				if (DEBUG_var < 0) { DEBUG_var = 0; }
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
	//gamestate = BEGINNING;
	//began_guessing = false;
	mines_remain = NUM_MINES_var;
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
	// print the fully-revealed field to screen only if DEBUG_var==2
	print_field(1, DEBUG_var);


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
	zerolist.sort(compare_by_position);
	unklist.sort(compare_by_position);
	// don't really need to sort the lists, because the find-and-remove method iterates linearly instead of using
	//   a BST, but it makes me feel better to know its sorted. and I only sort it once per game, so its fine.
}


// play one game with the field and zerolist as they currently are
// this way it's easier to lose from anywhere, without breaking out of loops
// return 1 = win, return 0 = loss
inline int play_game() {
	int r; // holds return value of any 'reveal' calls
	int numhunts = 0; // how many consecutive hunts
	int numguesses = 0; // how many consecutive guesses
	char buffer[6];

	 // reveal one cell (chosen at random or guaranteed to succeed)
	if (!HUNT_FIND_ZEROS_var) {
		// reveal a random cell (game loss is possible!)
		r = reveal(rand_from_list(&unklist));
		if (r == -1) { // no need to log it, first-move loss when random-hunting is a handled situation
			return 0;
		}
	} else {
		// reveal a cell from the zerolist... game loss probably not possible, but whatever
		r = reveal(rand_from_list(&zerolist));
		if (r == -1) {
			myprintfn(2, "Unexpected loss during initial zerolist reveal, must investigate!!\n");
			return 0;
		}
	}
	// add first entry to transition map
	mygamestats.trans_map += "^ ";
	print_field(3, DEBUG_var); print_gamestats(DEBUG_var);

	

	// begin game-loop, continue looping until something returns
	while (1) {
		
		int action = 0; // flag indicating some action was taken
		int numactions = 0; // how many actions have been taken during this stage
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
						r = set_flag(unk[i]);
						if (r == 1) {
							// validated win! time to return!
							// add entry to transition map, use 'numactions'
							sprintf_s(buffer, "s%i ", (numactions + action + 1));
							mygamestats.trans_map += buffer;
							return 1;
						}
					}
					action += 1;
					unk = filter_adjacent(unk, UNKNOWN); // must update the unknown list
				}
				
				// strategy 2: if an X-adjacency cell is next to X flags, all remaining unknowns are NOT flags and can be revealed
				if (me->effective == 0) {
					// reveal all adjacent unknown squares
					for (int i = 0; i < unk.size(); i++) {
						r = reveal(unk[i]);
						if (r == -1) {
							myprintfn(2, "Unexpected loss during SC satisfied-reveal, must investigate!!\n");
							return 0;
						}
						action += bool(r); // reduce whatever value it returned to either 0 or 1
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

		if (numactions != 0) {
			numhunts = 0; numguesses = 0;
			// add entry to transition map, use 'numactions'
			sprintf_s(buffer, "s%i ", numactions);
			mygamestats.trans_map += buffer;
			print_field(3, DEBUG_var); print_gamestats(DEBUG_var); // post-singlecell print
		}

		////////////////////////////////////////////
		// begin multi-cell logic (run through once? multiple times? haven't decided)
		// (currently only runs through exactly once)
		numactions = 0;
		mygamestats.gamestate = MULTICELL;
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
				} else if (r == 1) {// some cells were revealed!
					mygamestats.strat_121++;
					action += r;
				} else if (r == -1) {// unexpected game loss?
					// logged inside the function
					return 0;
				}

				// strategy 4: nonoverlap-flag
				r = strat_nonoverlap_flag(me);
				if (r == 0) {// nothing happened!
					// skip
				} else if (r == 1) {// some cells were flagged!
					mygamestats.strat_nov_flag++;
					action += r;
				} else if (r == 100) {// game won!
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
					mygamestats.strat_nov_safe += r;
					action += r;
				} else if (r == -1) {// unexpected game loss?
					// logged inside the function
					return 0;
				}


			} }
			if (action != 0) {
				numactions += action;
				mygamestats.began_solving = true;
			} else {
				break;
			}

			//break; // remove this to make it loop until nothing changes

		}// end multi-cell logic loop

		if (numactions != 0) {
			numhunts = 0; numguesses = 0;
			// add entry to transition map, use 'numactions'
			sprintf_s(buffer, "m%i ", numactions);
			mygamestats.trans_map += buffer;
			print_field(3, DEBUG_var); print_gamestats(DEBUG_var); // post-multicell print

			// don't do hunting, instead go back to singlecell
		} else {
			// nothing changed during multi-cell, so reveal a new cell
			if (zerolist.empty()) {
				/////////////////////////////////////////////
				// begin GUESSING phase
				mygamestats.gamestate = GUESSING;
				if (mygamestats.began_guessing == false) {
					mygamestats.began_guessing = true;
					myprintfn(DEBUG_var, "No more zeros, must begin guessing\n");
					//print_field(3, DEBUG_var+1); print_gamestats(DEBUG_var+1);
				}
				// reveal one random cell (game loss is likely!)
				// To win when random-guessing, if every guess is successful, it will reveal information that the SC/MC
				// logic will use to place the final flags. So, the only way to win is by placing flags
				r = reveal(rand_unk());
				if (r == -1) { // no need to log it, loss when random-hunting is a handled situation
					return 0;
				}
				
				// add to stats and transition map
				mygamestats.times_guessing++;
				numguesses++;
				if (numguesses >= 2) { // if this is the second+ guess in a string of guesses, need to remove previous trans_map entry
					// method: (numguesses-1) to string, get length, delete that + 2 trailing chars from trans_map
					int m = (std::to_string(numguesses - 1)).size() + 2; // almost always going to be 3 or 4
					mygamestats.trans_map.erase(mygamestats.trans_map.size() - m, m);
				}
				sprintf_s(buffer, "G%i ", numguesses);
				mygamestats.trans_map += buffer;
			} else {
				/////////////////////////////////////////////
				// begin HUNTING phase
				mygamestats.gamestate = HUNTING;
				if (!HUNT_FIND_ZEROS_var) {
					// reveal one random cell (game loss is possible!)
					r = reveal(rand_unk());
					if (r == -1) { // no need to log it, loss when random-hunting is a handled situation
						return 0;
					}
				} else {
					// reveal one cell from the zerolist... game loss probably not possible, but whatever
					r = reveal(rand_from_list(&zerolist));
					if (r == -1) {
						myprintfn(2, "Unexpected loss during hunting zerolist reveal, must investigate!!\n");
						return 0;
					}
				}
				// add to stats and transition map
				mygamestats.times_hunting++;
				numhunts++;
				if (numhunts >= 2) { // if this is the second+ hunt in a string of hunts, need to remove previous trans_map entry
					// method: (numhunts-1) to string, get length, delete that + 2 trailing chars from trans_map
					int m = (std::to_string(numhunts - 1)).size() + 2; // almost always going to be 3 or 4
					mygamestats.trans_map.erase(mygamestats.trans_map.size() - m, m);
				}
				sprintf_s(buffer, "^%i ", numhunts);
				mygamestats.trans_map += buffer;
			}
			print_field(3, DEBUG_var); print_gamestats(DEBUG_var); // post-hunt/guess print
		}

	}

	// should never hit this
	myprintfn(2, "Hit end of 'play_game' function without returning 1 or 0, must investigate!!\n");
	return -1;
}



// ************************************************************************************************
int main(int argc, char *argv[]) {
	// full-run init:
	// parse and store the input args here
	// note to self: argc has # of input args, INCLUDING PROGRAM NAME, argv is pointers to c-strings
	if (parse_input_args(argc, argv)) {
		return 0; // abort execution
	}

	printf_s("beginning minesweeeper solver\n");

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

	// logfile header info: mostly everything from the #defines
	myprintfn(2, "Going to play %i games, with X/Y/mines = %i/%i/%i\n", NUM_GAMES_var, SIZEX_var, SIZEY_var, NUM_MINES_var);
	if (HUNT_FIND_ZEROS_var) {
		myprintfn(2, "Using 'hunting' method = always succeed (always uncover a zero if any remain)\n");
	} else {
		myprintfn(2, "Using 'hunting' method = human-like (can lose at any stage)\n");
	}

	if (RANDOM_USE_SMART_var) {
		myprintfn(2, "Using 'hunting-guessing' mode = intelligent (slower but increased winrate)\n");
	} else {
		myprintfn(2, "Using 'hunting-guessing' mode = guess randomly (lower winrate but faster)\n");
	}

	// seed random # generator
	int tt = 0;
	if (SPECIFY_SEED_var == 0) {
		// create a new seed from time, log it, apply it
		myprintfn(2, "Generating new seed(s)\n");
		std::chrono::milliseconds ms = std::chrono::duration_cast< std::chrono::milliseconds >(
			std::chrono::system_clock::now().time_since_epoch()
			);
		tt = ms.count();
	} else {
		tt = int(SPECIFY_SEED_var);
	}
	myprintfn(2, "Using seed %i for game 1\n", tt);
	fflush(logfile);
	srand(tt);

	// set up stat-tracking variables
	myrunstats = run_stats();

	// create 'empty' field, for pasting onto the 'live' field to reset
	//struct cell field_blank[SIZEX_var][SIZEY_var];
	std::vector<std::vector<struct cell>> field_blank;
	std::vector<struct cell> asdf; struct cell asdf2; // needed for using the 'resize' command
	field_blank.resize(SIZEX_var, asdf);
	for (int m = 0; m < SIZEX_var; m++) {
		field_blank[m].resize(SIZEY_var, asdf2);
		for (int n = 0; n < SIZEY_var; n++) {
			field_blank[m][n] = cell(m, n); // may need to change this out for "push_back"
		}
	}


	for (int game = 0; game < NUM_GAMES_var; game++) {

		// generate a new seed, log it, apply it... BUT only if not on the first game
		if (game != 0) {
			int f = 0;
			do {
				// must use CHRONO to get time in MS since epoch, since many games take <1s time(0) was returning the same result and seeding with the same value
				std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(
					std::chrono::system_clock::now().time_since_epoch()
					);
				f = ms.count();
			} while (f == tt); // just in case any games go WAY too fast, keep looping until MS returns a different time than the one used before
			myprintfn(DEBUG_var + 1, "Using seed %i for game %i\n", f, game + 1);
			srand(f);
			tt = f; // save it for comparing against on the next pass
		}

		// reset various global variable things
		reset_for_game(&field_blank);

		// self-explanatory
		generate_new_field();

		// play a game, capture the return value
		int r = play_game();


		myrunstats.strat_121_total += mygamestats.strat_121;
		myrunstats.strat_nov_flag_total += mygamestats.strat_nov_flag;
		myrunstats.strat_nov_safe_total += mygamestats.strat_nov_safe;
		// increment run results depending on gamestate and gameresult
		myrunstats.games_total++;
		if (r == 0) { // game loss
			mygamestats.trans_map += "X";
			myrunstats.games_lost++;
			if (mygamestats.began_solving == false) {
				myrunstats.games_lost_beginning++;
			} else if (mygamestats.gamestate == HUNTING) {
				myrunstats.games_lost_hunting++;
			} else if (mygamestats.gamestate == GUESSING) {
				myrunstats.games_lost_guessing++;
			} else {
				myrunstats.games_lost_unexpectedly++;
			}
		} else if (r == 1) { // game win
			mygamestats.trans_map += "W";
			myrunstats.games_won++;
			if (mygamestats.began_guessing == true) {
				myrunstats.games_won_guessing++;
			} else {
				myrunstats.games_won_noguessing++;
			}
		}
		// print/log single-game results (also to console if #debug)
		print_field(3, DEBUG_var+1);
		print_gamestats(DEBUG_var+1);

		// status tracker for impatient people
		printf_s("Finished game %i of %i\n", (game+1), NUM_GAMES_var);


	}

	// done with games!

	// calculate total time elapsed and format for display
	std::chrono::milliseconds ms = std::chrono::duration_cast< std::chrono::milliseconds >(
		std::chrono::system_clock::now().time_since_epoch()
		);
	long f = ms.count();
	int elapsed_ms = int(f - myrunstats.start); // currently in ms
	double elapsed_sec = double(elapsed_ms) / 1000.; // seconds with a decimal

	double sec = fmod(elapsed_sec, 60.);
	int elapsed_min = int((elapsed_sec - sec) / 60.);
	int min = elapsed_min % 60;
	int hr = (elapsed_min - min) / 60;
	char timestr[20];
	sprintf_s(timestr, "%i:%02i:%05.3f", hr, min, sec);

	// print/log overall results (always print to terminal and log)
	myprintfn(2, "\nDone playing all %i games, displaying results! Time = %s\n\n", NUM_GAMES_var, timestr);
	myprintfn(2, "Games used X/Y/mines = %i/%i/%i\n", SIZEX_var, SIZEY_var, NUM_MINES_var);
	if (HUNT_FIND_ZEROS_var) {
		myprintfn(2, "Used 'hunting' method = always succeed (always uncover a zero if any remain)\n");
	} else {
		myprintfn(2, "Used 'hunting' method = human-like (can lose at any stage)\n");
	}
	if (RANDOM_USE_SMART_var) {
		myprintfn(2, "Used 'hunting-guessing' mode = intelligent (slower but increased winrate)\n");
	} else {
		myprintfn(2, "Used 'hunting-guessing' mode = guess randomly (lower winrate but faster)\n");
	}
	myprintfn(2, "Average time per game:                   %8.4f sec\n", (float(elapsed_sec) / float(myrunstats.games_total)));
	if (RANDOM_USE_SMART_var) {
	myprintfn(2, "Smartguess border est. avg deviation:     %+7.4f\n", (float(myrunstats.smarguess_maxret_diff) / float(myrunstats.smartguess_attempts)));
	}
	myprintfn(2, "Average 121-cross uses per game:         %5.1f\n", (float(myrunstats.strat_121_total) / float(myrunstats.games_total)));
	myprintfn(2, "Average nonoverlap-flag uses per game:   %5.1f\n", (float(myrunstats.strat_nov_flag_total) / float(myrunstats.games_total)));
	myprintfn(2, "Average nonoverlap-safe uses per game:   %5.1f\n\n", (float(myrunstats.strat_nov_safe_total) / float(myrunstats.games_total)));
	myprintfn(2, "Total games played:                     %6i\n", myrunstats.games_total);
	if (myrunstats.games_with_eights != 0) {
	myprintfn(2, "    Games with 8-adj cells:              %5i\n", myrunstats.games_with_eights);
	}
	myprintfn(2, "    Total games won:                     %5i   %5.1f%%    -----\n", myrunstats.games_won,              (100. * float(myrunstats.games_won) / float(myrunstats.games_total)));
	myprintfn(2, "        Games won without guessing:      %5i   %5.1f%%   %5.1f%%\n", myrunstats.games_won_noguessing,  (100. * float(myrunstats.games_won_noguessing) / float(myrunstats.games_total)),  (100. * float(myrunstats.games_won_noguessing) / float(myrunstats.games_won)));
	myprintfn(2, "        Games won that required guessing:%5i   %5.1f%%   %5.1f%%\n", myrunstats.games_won_guessing,    (100. * float(myrunstats.games_won_guessing) / float(myrunstats.games_total)),    (100. * float(myrunstats.games_won_guessing) / float(myrunstats.games_won)));
	myprintfn(2, "    Total games lost:                    %5i   %5.1f%%    -----\n", myrunstats.games_lost,             (100. * float(myrunstats.games_lost) / float(myrunstats.games_total)));
	myprintfn(2, "        Games lost in the first move(s): %5i   %5.1f%%   %5.1f%%\n", myrunstats.games_lost_beginning,  (100. * float(myrunstats.games_lost_beginning) / float(myrunstats.games_total)),  (100. * float(myrunstats.games_lost_beginning) / float(myrunstats.games_lost)));
	myprintfn(2, "        Games lost in midgame hunting:   %5i   %5.1f%%   %5.1f%%\n", myrunstats.games_lost_hunting,    (100. * float(myrunstats.games_lost_hunting) / float(myrunstats.games_total)),    (100. * float(myrunstats.games_lost_hunting) / float(myrunstats.games_lost)));
	myprintfn(2, "        Games lost in lategame guessing: %5i   %5.1f%%   %5.1f%%\n", myrunstats.games_lost_guessing,   (100. * float(myrunstats.games_lost_guessing) / float(myrunstats.games_total)),   (100. * float(myrunstats.games_lost_guessing) / float(myrunstats.games_lost)));
	if (myrunstats.games_lost_unexpectedly != 0) {
	myprintfn(2, "        Games lost unexpectedly:         %5i   %5.1f%%   %5.1f%%\n", myrunstats.games_lost_unexpectedly, (100. * float(myrunstats.games_lost_unexpectedly) / float(myrunstats.games_total)), (100. * float(myrunstats.games_lost_unexpectedly) / float(myrunstats.games_lost)));
	}
	myprintfn(2, "\n");

	
	fflush(logfile);

	fclose(logfile);
	system("pause");
    return 0;
}

