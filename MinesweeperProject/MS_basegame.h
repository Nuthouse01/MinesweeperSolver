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




// this info is not at all secretive; shouldn't be changed after the runs begin, but you can't cheat by using or changing this info.
class runinfo {
public:
	runinfo();
	int NUM_GAMES_var;
	int SIZEX_var;
	int SIZEY_var;
	int NUM_MINES_var;
	int SPECIFY_SEED_var;
	int SCREEN_var;
	FILE * logfile;
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
	game(); // empty constructor, doesn't set up field_blank
	game(int x, int y); // sets up field_blank
	std::list<class cell *> zerolist; // zero-list, probably should be private but I dont care to figure it out
	std::list<class cell *> unklist; // unknown-list, hopefully allows for faster "better rand"
	std::vector<std::vector<class cell>> field; // the actual playing field; overwritten for each game
	std::vector<std::vector<class cell>> field_blank;

	unsigned int get_mines_remaining();
	inline class cell * cellptr(int xxx, int yyy);
	std::vector<class cell *> get_adjacent(class cell * me);
	std::vector<class cell *> filter_adjacent(class cell * me, cell_state target);
	std::vector<class cell *> filter_adjacent(std::vector<class cell *> adj, cell_state target);

	int reveal(class cell * me);
	int set_flag(class cell * me);
	void print_field(int mode, int screen);
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
	short unsigned int value; // NOTE: making this public for smartguess_diff stat only
	cell();
	short unsigned int x, y;
	short unsigned int get_value();
	short unsigned int get_effective();
	cell_state get_status();
	void set_status_satisfied();

	friend int game::reveal(class cell * me);
	friend int game::set_flag(class cell * me);
	friend void game::print_field(int mode, int screen);
	friend int game::reset_for_game();
};





void myprintfn(int p, const char* fmt, ...);
inline bool sort_by_position(class cell * a, class cell * b);
inline int compare_two_cells(class cell * a, class cell * b);



// NOTE: MUST BE HERE IN THE HEADER, templates can't have separate declarations/definitions
// return a random object from the provided list, or NULL if the list is empty
template <class foobar> foobar rand_from_list(std::list<foobar> * fromme) {
	if (fromme->empty()) { return NULL; } // NOTE: bad if this is supposed to return an iterator!!
	int f = rand() % fromme->size();
	std::list<foobar>::iterator iter = fromme->begin();
	for (int i = 0; i < f; i++) { iter++; } // iterate to this position
	return *iter;
}

template <class ttttt> ttttt rand_from_vect(std::vector<ttttt> * fromme) {
	if (fromme->empty()) { return NULL; } // NOTE: bad if this is supposed to return an iterator!!
	int f = rand() % fromme->size();
	return (*fromme)[f];
}


#endif
