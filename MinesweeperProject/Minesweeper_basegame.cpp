// base minesweeper game essentials: probably shouldn't be modified by anyone implementing their own solver
// Brian Henson 7/3/2018


// TODO: selectively remove these includes to whittle down what is actually needed/used in 'basegame'
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





#include "Minesweeper_basegame.h" // include myself


// basic constructor
runinfo::runinfo() {
	NUM_GAMES_var = 0;
	SIZEX_var = 0;
	SIZEY_var = 0;
	NUM_MINES_var = 0;
	SPECIFY_SEED_var = 0;
	SCREEN_var = 0;
	FILE * logfile = NULL;
}



void myprintfn(int p, const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	if (p >= 1) { vfprintf(myruninfo.logfile, fmt, args); }
	if (p >= 2) { vprintf(fmt, args); }
	va_end(args);
}



// constructor: with no args, don't do much
game::game() {
	mines_remaining = 0;
	zerolist.clear();
	unklist.clear();
	field.clear();
	field_blank.clear();
}
// constructor: with args, allocate the field to be the proper size
game::game(int xxx, int yyy) {
	game();
	// create 'empty' field, for pasting onto the 'live' field to reset
	class cell asdf2 = cell(); // needed for using the 'resize' command, I will overwrite the actual coords later
	std::vector<class cell> asdf = std::vector<class cell>(); // init empty
	asdf.resize(yyy, asdf2);									// fill it with copies of asdf2
	asdf.shrink_to_fit();										// set capacity exactly where i want it, no bigger
	field_blank = std::vector<std::vector<class cell>>();		// init empty
	field_blank.resize(xxx, asdf);								// fill it with copies of asdf
	field_blank.shrink_to_fit();								// set capacity exactly where i want it, no bigger
	for (int m = 0; m < xxx; m++) {
		for (int n = 0; n < yyy; n++) {
			(field_blank[m][n]).x = m; // overwrite 0,0 with the correct coords
			(field_blank[m][n]).y = n; // overwrite 0,0 with the correct coords
		}
	}
}

unsigned int game::get_mines_remaining() {
	return mines_remaining;
}



cell::cell() { // constructor
	x = 0;
	y = 0;
	status = UNKNOWN;
	value = 0;
	effective = 0;
}
short unsigned int cell::get_value() {
	if (status == VISIBLE) { return value; } else { myprintfn(2, "HEY! NO CHEATING!\n"); return 100; }
}
short unsigned int cell::get_effective() {
	if (status == VISIBLE) { return effective; } else { myprintfn(2, "HEY! NO CHEATING!\n"); return 100; }
}
cell_state cell::get_status() {
	return status;
}
void cell::set_status_satisfied() {
	status = SATISFIED;
}





// cellptr: if the given X and Y are valid, returns a pointer to the cell; otherwise returns NULL
// during single-cell and multi-cell iterations, just use &field[x][y] because the X and Y are guaranteed not off the edge
inline class cell * game::cellptr(int x, int y) {
	if ((x < 0) || (x >= myruninfo.SIZEX_var) || (y < 0) || (y >= myruninfo.SIZEY_var))
		return NULL;
	return &field[x][y];
}

// adjacent: returns 3/5/8 adjacent cells, regardless of content or status
// the order of the cells in the list will always be the same!!
std::vector<class cell *> game::get_adjacent(class cell * me) {
	std::vector<class cell *> adj_list;
	adj_list.reserve(8); // resize it once since this will almost always be size 8
	int x = me->x;
	int y = me->y;
	class cell * z;
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
// also calculates the 'effective' value of the freshly-revealed cell
// if it's a zero, remove it from the zero-list and recurse
// doesn't complain if you give it a null pointer
int game::reveal(class cell * revealme) {
	if ((revealme == NULL) || (revealme->status != UNKNOWN))
		return 0; // if it is somehow null, or flagged, satisfied, or already visible, then do nothing. will happen often.

	revealme->status = VISIBLE;
	unklist.remove(revealme);
	if (revealme->value == MINE) {
		// lose the game, handled wherever calls here
		return -1;
	}
	// if not a mine, set the freshly-revealed 'effective' value
	std::vector<cell *> adjflag = filter_adjacent(revealme, FLAGGED);
	revealme->effective = revealme->value - adjflag.size();

	// if it's an effective zero, change status to SATISFIED and recurse, revealing adjacent cells
	if (revealme->effective == 0) {
		revealme->status = SATISFIED;
		//if a true zero, remove it from the zero-list
		if (revealme->value == 0) { zerolist.remove(revealme); }
		int retme = 0;
		int t = 0;
		std::vector<class cell *> adj = filter_adjacent(revealme, UNKNOWN);
		for (std::vector<class cell *>::iterator ptr = adj.begin(); ptr < adj.end(); ptr++) {
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
		return 1;
	}
	return 0; // dangling return just in case
}

// flag: sets as flagged, reduces # remaining mines, reduce "effective" values of everything visible around it
// if remaining_mines = 0, validate the win; if it passes, return 1 for win! if it fails, something went wrong!
// if remaining_mines != 0, return 0 and continue solving
// if not an actual mine, dump a bunch of info and abort
int game::set_flag(class cell * flagme) {
	if (flagme->status != UNKNOWN) {
		return 0;
	}

	// check if it flagged a non-mine square, and if so, why?
	if (flagme->value != MINE) {
		myprintfn(2, "ERR: FLAGGED A NON-MINE CELL\n");
		assert(flagme->value == MINE);
		system("pause");
	}

	unklist.remove(flagme);
	flagme->status = FLAGGED; // set it to flagged, like it should be


	// decrement "effective" values of everything visible around it
	std::vector<class cell *> adj = filter_adjacent(flagme, VISIBLE);
	for (int i = 0; i < adj.size(); i++) {
		adj[i]->effective--;
	}

	// decrement the remaining mines
	mines_remaining -= 1;
	if (mines_remaining == 0) {
		//try to validate the win: every mine is flagged, and every not-mine is not-flagged
		for (int y = 0; y < myruninfo.SIZEY_var; y++) {
			for (int x = 0; x < myruninfo.SIZEX_var; x++) { // iterate over each cell
				class cell * v = &field[x][y];

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
			}
		}
		// officially won!
		return 1;
	}

	return 0;
}

// filter_adjacent: basically the same as get_adjacent but returns only the TARGET cells
// can plausibly return an empty vector
// this version takes a cell pointer
std::vector<class cell *> game::filter_adjacent(class cell * me, cell_state target) {
	std::vector<class cell *> adj = get_adjacent(me);
	return filter_adjacent(adj, target);
}


// filter_adjacent: basically the same as get_adjacent but returns only the TARGET cells
// can plausibly return an empty vector
// this version takes an adjacency vector
std::vector<class cell *> game::filter_adjacent(std::vector<class cell *> adj, cell_state target) {
	int i = 0;
	while (i < adj.size()) {
		if (adj[i]->get_status() != target)
			adj.erase(adj.begin() + i);
		else
			i++;
	}
	return adj;
}



// print: either 1) fully-revealed field, 2) in-progress field as seen by human, 3) in-progress field showing 'effective' values
// borders made with +, zeros: blank, adjacency (or effective): number, unknown: -, flag or mine: *
// if SCREEN=0, don't print anything. if SCREEN=1, print to log. if SCREEN=2, print to both.
void game::print_field(int mode, int screen) {
	if (screen <= 0) return;
	if (mode == 1) {
		myprintfn(screen, "Printing fully-revealed field\n");
	} else if (mode == 2) {
		myprintfn(screen, "Printing in-progress field seen by humans\n");
	} else if (mode == 3) {
		myprintfn(screen, "Printing in-progress field showing effective values\n");
	}
	// build and print the field one row at a time

	// top/bottom:
	std::string top = std::string(((myruninfo.SIZEX_var + 2) * 2) - 1, '+') + "\n";
	myprintfn(screen, top.c_str());

	std::string line;
	for (int y = 0; y < myruninfo.SIZEY_var; y++) {
		for (int x = 0; x < myruninfo.SIZEX_var; x++) {
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
	fflush(myruninfo.logfile);
}




// reset_for_game: reset the field, place new mines, clear and repopulate the lists, etc
// returns the number of 8-cells found when generating (just because I can)
int game::reset_for_game() {
	// reset the 'live' field
	//memcpy(field, field_blank, SIZEX_var * SIZEY_var * sizeof(class cell)); // paste
	field = field_blank; // paste

	zerolist.clear(); // reset the list
	unklist.clear();
	mines_remaining = myruninfo.NUM_MINES_var;

	// generate the mines
	for (int i = 0; i < myruninfo.NUM_MINES_var; i++) {
		int x = rand() % myruninfo.SIZEX_var; int y = rand() % myruninfo.SIZEY_var;
		if (field[x][y].value == MINE) {
			i--; continue; // if already a mine, generate again
		}
		field[x][y].value = MINE;
		field[x][y].effective = MINE; // why not
	}

	int eights = 0;
	// set up adjacency values, also populate zero-list and unk-list
	for (int y = 0; y < myruninfo.SIZEY_var; y++) {
		for (int x = 0; x < myruninfo.SIZEX_var; x++) { // iterate over the field
			unklist.push_back(&field[x][y]); // everything starts out in the unklist

			if (field[x][y].value == MINE)
				continue; // don't touch existing mines
			std::vector<class cell *> adj = get_adjacent(&field[x][y]);
			int t = 0;
			for (int j = 0; j < adj.size(); j++) {
				if (adj[j]->value == MINE) { t++; }
			}
			field[x][y].value = t;
			field[x][y].effective = t;

			if (t == 0) {
				zerolist.push_back(&field[x][y]);
			}
			if (t == 8) {
				myprintfn(2, "Found an 8 cell when generating, you must be lucky! This is incredibly rare!\n");
				eights++;
				//myrunstats.games_with_eights++;
			}
		}
	}

	// print the fully-revealed field to screen only if SCREEN_var==2
	print_field(1, myruninfo.SCREEN_var);

	zerolist.sort(sort_by_position);
	unklist.sort(sort_by_position);
	// don't really need to sort the lists, because the find-and-remove method iterates linearly instead of using
	//   a BST, but it makes me feel better to know its sorted. and I only sort it once per game, so its fine.
	return eights;
}




// if a goes first, return negative; if b goes first, return positive; if identical, return 0
inline int compare_two_cells(class cell * a, class cell * b) {
	return ((b->y * myruninfo.SIZEX_var) + b->x) - ((a->y * myruninfo.SIZEX_var) + a->x);
}

// if a goes before b, return true... needed for consistient sorting
inline bool sort_by_position(class cell * a, class cell * b) {
	if (compare_two_cells(a, b) < 0) { return true; } else { return false; }
}

