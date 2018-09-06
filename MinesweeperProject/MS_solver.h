#ifndef MS_SOLVER
#define MS_SOLVER
// Brian Henson 7/4/2018
// this file contains the solver code/logic as implemented by whoever is writing it
// also includes the stat-tracking structs




#include <cstdlib> // rand, other stuff
#include <cstdio> // printf
#include <vector> // used
#include <list> // used
#include <cassert> // so it aborts when something wierd happens (but its optimized away in 'release' mode)
#include <algorithm> // for pod-based intelligent recursion
#include <utility> // for 'pair' objects
#include <Windows.h> // adds min() and max() macros





extern class runinfo myruninfo;
extern class game mygame;
extern int GUESSING_MODE_var;




/* terminology:
solution: one way to satisfy a chain
	answer: how many mines are in a solution
	allocation: the specific way of placing flags in a chain that constitutes a solution (may be partial or complete)
	allocs_encompassed: how many similar (but unique) allocations this solution encompasses
podwise_return: the list of solutions found
	effort: the number of solutions found, represents time spent computing; separate from list size because entries may be removed if determined invalid
scenario: one way to saturate the chosen pod when iterating on a chain
	scenario_links_to_flag: the specific way to place flags (only on the links!) that constitutes a scenario
	allocs_encompassed: how many similar scenarios this single scenario encompasses
*/


///////////////////////////////////////////////////////////////////////////////////////////////
// struct declarations without the actual contents
// new pod-based architecture to handle the smartguess madness, MASSIVELY object-based unlike how it was before

// a solution is one way to satisfy a chain
struct solutionobj {
	solutionobj() {};
	solutionobj(float ans, int howmany);

	float answer; // the number of mines determined to be in the chain 
	int allocs_encompassed; // how many allocations this answer corresponds to
	std::list<class cell *> allocation; // cells to flag to apply this solution; may only be partial
};

// used in 'perfectmode' to count how many times a cell is flagged across all possible solutions
struct aggregate_cell {
	aggregate_cell();
	aggregate_cell(class cell * newme, int newtf);

	class cell * me;
	int times_flagged;
};


// holds the list of all solutions found, as a struct for some extra utility
struct podwise_return {
	podwise_return();
	podwise_return(float ans, int howmany);

	std::list<struct solutionobj> solutions; // list of all allocations found; unsorted, includes many duplicates
	int effort; // represents # of solutions found, preserved even after .avg function
	std::list<struct aggregate_cell> agg_info;
	int agg_allocs;

	inline int size();
	inline float efficiency();
	inline struct podwise_return& operator+=( class cell * & addlist);
	inline struct podwise_return& operator+=(const std::vector<class cell *>& addvect);
	inline struct podwise_return& operator+=(const std::list<class cell *>& addlist);
	inline struct podwise_return& operator*=(const int& rhs);
	inline struct podwise_return& operator+=(const int& rhs);
	float avg();
	float max_val();
	float min_val();
	int total_alloc();
	void add_aggregate(class cell * newcell, float times_flagged);
};


// represents an overlap cell shared by 2 or more pods
struct link {
	link() {};
	link(class cell * shared, class cell * shared_root);

	class cell * link_cell;					// the cell in question
	std::list<class cell *> linked_roots;	// the roots of any pods it is part of

	inline bool operator== (const struct link o) const; // need this to use the 'remove by value' function
};


// represents the adjacent unknown cells of a visible adjacency number
// almost certain to have some overlap with other pods; these are recorded as 'links'
struct pod {
	pod() {};
	pod(class cell * new_root);

	class cell * root;  //the visible adjacency-cell the pod is based on (or one of them if there were dupes)
	int mines;			//how many mines are in the pod
	std::list<struct link> links; //cells shared by other pods... list because will often remove from this
	std::vector<class cell *> cell_list;  //all the cells in the pod, including link cells...
	int cell_list_size; // number of cells in the pod, including link cells... NOTE: only used INSIDE recursion!
	int chain_idx;		// used to identify chains... NOTE: only used OUTSIDE recursion!

	float risk();
	int size();
	int scan();
	void add_link(class cell * shared, class cell * shared_root);
	int remove_link(class cell * shared, bool isaflag);
	std::list<struct scenario> pod::find_scenarios(bool use_t2_opt);
	std::list<struct link>::iterator get_link(int l);
};


// chain = a group of pods. when recursing, a chain begins as a group of interconnected/overlapping pods...
// when not recursing its probably the "master chain" which isn't a chain at all, just the set of all pods
struct chain {
	chain();

	std::list<struct pod> podlist; // the only member, a list of (probably) overlapping pods

	std::list<struct pod>::iterator root_to_pod(class cell * linked_root);
	std::list<struct pod>::iterator int_to_pod(int f);
	std::vector<std::list<struct pod>::iterator> get_5x5_around(std::list<struct pod>::iterator center, bool include_corners);
	std::vector<struct chain> sort_into_chains(int r, bool reduce);
	int identify_chains();
	void identify_chains_recurse(int idx, struct pod * me);
};



// an immitation of the 'field' object, to be allocated globally?? and reused each time the pod-smart-guess is used
// holds the risk info for each cell, will calculate the border cells with the lowest risk
struct riskholder {
	riskholder() {};
	riskholder(int x, int y);

	std::vector<std::vector<std::vector<float>>> riskarray; // immitation of the 'field' for holding risk info, can be quickly accessed with x and y

	void addrisk(class cell * foo, float newrisk);
	std::pair<float, std::list<class cell *>> findminrisk();
	float finalrisk(int x, int y);
};


// bundles the list of links to flag with an allocation #
// NOTE: this could be a pair/tuple but since it is actually stored I'll leave it as a struct
struct scenario {
	scenario() {};
	scenario(std::list<std::list<struct link>::iterator> map, int num);

	std::list<std::list<struct link>::iterator> scenario_links_to_flag; // list of link-list-iterators to flag
	int allocs_encompassed; // how many possible allocations this scenario stands in for
};






////////////////////////////////// small but essential utility functions

// compare lists of cells
inline int compare_two_lists_of_cells(std::list<class cell *> a, std::list<class cell *> b);
// use compare_two_lists_of_cells to sort links, without caring about their shared_cell member
bool sort_links_blind(std::list<struct link>::iterator a, std::list<struct link>::iterator b);
// compare lists of links (lists of lists of cells), while ignoring the shared_cell member
inline int compare_two_pre_scenarios(std::list<std::list<struct link>::iterator> a, std::list<std::list<struct link>::iterator> b);
// use compare_two_pre_scenarios to sort
bool sort_pre_scenarios(std::list<std::list<struct link>::iterator> a, std::list<std::list<struct link>::iterator> b);
// use compare_two_pre_scenarios to uniquify
bool equivalent_pre_scenarios(std::list<std::list<struct link>::iterator> a, std::list<std::list<struct link>::iterator> b);
// use compare_two_cells to sort
bool sort_aggregate_cell(struct aggregate_cell a, struct aggregate_cell b);
// use compare_two_cells to uniquify
bool equivalent_aggregate_cell(struct aggregate_cell a, struct aggregate_cell b);

// determine all ways to choose K from N, return a VECTOR of VECTORS of INTS ranging from 0 to N-1
std::list<std::vector<int>> comb(int K, int N);
// return the # of total combinations you can choose K from N, simple math function
inline int comb_int(int K, int N);
// simple math function, i never need to know higher than 8! so just hardcode the answers
inline int factorial(int x);




////////////////////////////////// functions for the recursive smartguess method(s)

// build the master chain that will be used for smartguess... encapsulates steps 1/2/3 of original 11-stage plan
// also apply "multicell" versions of nonoverlap-safe and nonoverlap-flag; partial matches and chaining logic rules together
// may identify some cells as "definite safe" or "definite mine", and will reveal/flag them internally
// if no cells are flagged/cleared, will output the master chain thru input arg for passing to full recursive function below
// return: 1=win/-1=loss/0=continue (winning is rare but theoretically possible, but cannot lose unless something is seriously out of whack)
int strat_multicell_logic_and_chain_builder(struct chain * buildme, int * thingsdone);
// smartguess: searches for chain solutions with only one allocation (unique) to apply
// perfectmode: ^ plus, if non-unique solution is found, eliminate all other solutions to that chain
//				^ plus, searches for chain solutions that are definitely invalid and eliminates them
// return: 1=win/-1=loss/0=continue (cannot lose unless something is seriously out of whack)
// but, it only causes smartguess to return to the main play_game level if this function adds to *thingsdone
int strat_endsolver_and_solution_reducer_logic(std::vector<struct podwise_return> * prvect, std::list<class cell *> * interior_list, int * thingsdone);
// laboriously determine the % risk of each unknown cell and choose the one with the lowest risk to reveal
// can completely solve the puzzle, too; if it does, it clears/flags everything it knows for certain
// doesn't return cells, instead clears/flags them internally
// modeflag: 0=guess, 1=multicell, 2=endsolver
// return: 1=win/-1=loss/0=continue/-2=unexpected loss (winning is unlikely but possible)
int smartguess(struct game_stats * gstats, int * thingsdone, int * modeflag);
// recursively operates on a interconnected 'chain' of pods, and returns the list of all allocations it can find.
// 'recursion_safety_valve' is set whenever it would return something resulting from 10k or more solutions, then from that point
//		till the chain is done, it checks max 2 scenarios per level.
// smartguess/normal: returns list of solutionvalues w/ weights
// smartguess/endsolver: returns reduced/compressed/simplified roadmaps for the solutions + list of solutionvalues w/ weights
// perfectmode/normal: returns aggregate data for each cell in the chain + list of solutionvalues w/ weights
// perfectmode/endsolver: returns exhaustive list of all roadmaps, WAY more memory usage, dont even worry about optimizing it
//		^ don't store/calculate aggregate data, synthesize the data after step 8 because step 8 will eliminate some of the solutions
struct podwise_return podwise_recurse(int rescan_counter, int mines_from_above, struct chain * mychain, bool use_endsolver);



////////////////////////////////// single-cell logic

// firstly, if an X-effective cell is next to X unknowns, flag them all.
// secondly, if an X-adjacency cell is next to X flags (AKA effective = 0), all remaining unknowns are SAFE and can be revealed.
// veryveryvery simple to understand. internally identifies and flags/clears cells. also responsible for setting state to "satisfied"
// when appropriate. no special stats to track here, except for the "singlecell total action count".
// return: 1=win/-1=loss/0=continue (cannot lose tho, unless something is seriously out of whack)
int strat_singlecell(class cell * me, int * thingsdone);


////////////////////////////////// two-cell strategies

// looks for a specific arrangement of visible/unknown cells; if found, I can clear up to 2 cells.
// unlike the other strategies, this isn't based in logic so much... this is just a pattern I noticed.
// return: 1=win/-1=loss/0=continue (except cannot win, ever, and cannot lose unless something is seriously out of whack)
int strat_121_cross(class cell * center, struct game_stats * gstats, int * thingsdone);
//If two cells are X and X + 1, and the unknowns around X + 1 fall inside unknowns around X except for ONE, that
//non-overlap cell must be a mine
//Expanded to the general case: if two cells are X and X+Z, and X+Z has exactly Z unique cells, then all those cells must be mines
// Compare against 5x5 region minus corners.
// X(other) = 1/2/3/4,  Z = 1/2/3/4/5/6
// return: 1=win/-1=loss/0=continue (except cannot lose here because it doesn't reveal cells here)
int strat_nonoverlap_flag(class cell * center, struct game_stats * gstats, int * thingsdone);
//If the adj unknown tiles of a 1/2/3 -square are a pure subset of the adj unknown tiles of another
//square with the same value, then the non-overlap section can be safely revealed!
//Compare against any other same-value cell in the 5x5 region minus corners
// return: 1=win/-1=loss/0=continue (except cannot win, ever, and cannot lose unless something is seriously out of whack)
int strat_nonoverlap_safe(class cell * center, struct game_stats * gstats, int * thingsdone);





#endif