#ifndef MS_BASEGAME
#define MS_BASEGAME
// Brian Henson 7/3/2018
// base minesweeper game essentials: probably shouldn't be modified by anyone implementing their own solver



#include <cstdlib> // rand, other stuff
#include <cstdio> // file pointer SUPPOSED to be defined here, but it works even without this? whatever
#include <string> // for print_field
#include <vector> // used
#include <list> // used
#include <cassert> // so it aborts when something wierd happens (but its optimized away in 'release' mode)
#include <cstdarg> // for variable-arg function-macro



// essential stuff like # of games to play, field size, # of mines, etc
// this info is not at all secretive; shouldn't be changed after the runs begin, but you can't cheat by using or changing this info.
// TODO: for even more security, make the 'game' object a private member of runinfo with an accessor function? it's an idea
// or perhaps make 'runinfo' a member of 'game' object in the same way???
class runinfo {
private:
	int SIZEX; // these 3 can be set only once, but can be read whenever
	int SIZEY;
	int NUM_MINES;
public:
	// basic constructor
	runinfo();

	int NUM_GAMES;
	int SPECIFY_SEED;
	int SCREEN;
	FILE * logfile;

	// should be called once to set the x/y/mines, but no more than that! don't allow redefinitions
	void set_gamedata(int newx, int newy, int newmines);
	// read-only accessor
	inline int get_SIZEX() { return SIZEX; }
	// read-only accessor
	inline int get_SIZEY() { return SIZEY; }
	// read-only accessor
	inline int get_NUM_MINES() { return NUM_MINES; }
};
extern class runinfo myruninfo;


#define MINE 50
enum cell_state : short {
	UNKNOWN,			// cell contents are not known
	SATISFIED,			// all 8 adjacent cells are either flagged or visible; no need to think about this one any more
	VISIBLE,			// cell has been revealed; hopefully contains an adjacency number
	FLAGGED				// logically determined to be a mine (but it don't know for sure)
};


class cell;


// holds the essential structure and some fundamental functions for playing minesweeper code-wise with no UI
// has some private settings to cell contents can be read only when visible, etc, ensure no cheating!
class game {
private:
	unsigned int mines_remaining; // can be read but not written
public:
	// empty constructor: with no args, don't do much
	game();
	// init: with args, allocate the field to be the proper size
	int init(int x, int y);

	std::list<class cell *> zerolist; // zero-list, probably should be private but I dont care to figure it out
	std::list<class cell *> unklist; // unknown-list, hopefully allows for faster "better rand"
	std::vector<std::vector<class cell>> field; // the actual playing field; overwritten for each game
	std::vector<std::vector<class cell>> field_blank; // a blank field, easier to copy this onto actual than to erase & rebuild

	// read-only accessor
	inline unsigned int get_mines_remaining() { return mines_remaining; }
	// checks x and y against field size; if valid, return cell pointer. if invalid, return NULL
	class cell * cellptr(int xxx, int yyy);
	// return a vector of the 3/5/8 cells surrounding the target
	std::vector<class cell *> get_adjacent(class cell * me);
	// basically the same as get_adjacent but returns only cells of the given state
	std::vector<class cell *> filter_adjacent(class cell * me, cell_state target);
	// basically the same as get_adjacent but returns only cells of the given state
	std::vector<class cell *> filter_adjacent(std::vector<class cell *> adj, cell_state target);

	// uncovers the target cell, turning it from UNKNOWN to VISIBLE. also calculates the 'effective' value of the freshly-revealed cell
	// remove it from the unklist, and if it's a zero, remove it from the zero-list and recurse!
	// returns -1 if the cell was a mine (GAME LOSS), or the # of cells revealed otherwise
	int reveal(class cell * me);
	// sets cell state to FLAGGED, reduces # remaining mines, reduce "effective" values of everything visible around it
	// also checks if the game was won as a result
	// return -1=win, 0=nothing happened (target already flagged), 1=flagged a cell, 2=flagged a non-mine cell
	int set_flag(class cell * me);
	// print: either 1) fully-revealed field, 2) in-progress field as seen by human, 3) in-progress field showing 'effective' values
	// borders made with +, zeros= blank, adjacency (or effective)= number, unknown= -, flag or mine= *
	// if SCREEN=0, don't print anything. if SCREEN=1, print to log. if SCREEN=2, print to both.
	void print_field(int mode, int screen);
	// reset the field, place new mines, set 'value' for non-mine cells, clear and repopulate the lists, etc
	// returns the number of 8-cells found when generating (just because I can)
	// also prints the fully-revealed field after doing all this
	int reset_for_game();
};


// one square of the minesweeper grid
// aside from the friend functions, the cell content cannot be read unless the state is VISIBLE
// only the friend functions can change the value and effective stuff
// the solver can set the cell state to SATISFIED, and read the state variable, but only the friends can set it to FLAGGED or VISIBLE
class cell {
private:
	cell_state status;
	short unsigned int effective;
	//short unsigned int value; // TODO: implement bias function and make this private again
public:
	short unsigned int value; // TODO: made this public for smartguess_diff stat only
	cell();
	short unsigned int x, y;
	// read-only conditional accessor, only works if status == VISIBLE
	short unsigned int get_value();
	// read-only conditional accessor, only works if status == VISIBLE
	short unsigned int get_effective();
	// read-only accessor
	inline cell_state get_status() { return status; }
	// sets cell status to SATISFIED
	inline void set_status_satisfied() { status = SATISFIED; }

	friend int game::reveal(class cell * me);
	friend int game::set_flag(class cell * me);
	friend void game::print_field(int mode, int screen);
	friend int game::reset_for_game();
};





// basically printf that goes to both terminal and logfile, depending on the value of p
// 0=none, 1=log, 2=both
void myprintfn(int p, const char* fmt, ...);
// cells are sorted in reading-order, where 0,0 is top-left, +x is right and +y is down...
// if a goes first, return negative; if b goes first, return positive; if identical, return 0
inline int compare_two_cells(class cell * a, class cell * b);
// use compare_two_cells to sort
bool sort_cells(class cell * a, class cell * b);
// extract_overlap: takes two vectors of cells, returns (first_vect_unique) (second_vect_unique) (overlap)
// NOTE: i could templatize this function, but A) there's no need to, and B) I'd have to put it in the header
std::vector<std::vector<class cell *>> extract_overlap(std::vector<class cell *> me_unk, std::vector<class cell *> other_unk);
// return a random object from the provided list, or NULL if the list is empty
// NOTE: i could templatize this function, but A) there's no need to, and B) I'd have to put it in the header
class cell * rand_from_list(std::list<cell *> * fromme);



#endif
