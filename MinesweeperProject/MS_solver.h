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
extern bool FIND_EARLY_ZEROS_var;
extern bool RANDOM_USE_SMART_var;




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

// name
	// constructor(s)
	// member variables
	// member functions


// a solution is one way to satisfy a chain
struct solutionobj {
	solutionobj() {};
	solutionobj(float ans, int howmany);

	float answer; // the number of mines determined to be in the chain 
	int allocs_encompassed; // how many allocations this answer corresponds to
	std::list<class cell *> allocation; // cells to flag to apply this solution; may only be partial
};

// NOTE: could be a tuple/threeple, but its cleaner this way
// used in 'perfectmode' to count how many times a cell is flagged
// TODO: is there any point to having "allocs_encompassed" for each cell? it should always be the same across the board, right?
//		first try it this way, then try reducing it
struct aggregate_cell {
	aggregate_cell();
	aggregate_cell(class cell * newme, int newtf);
	aggregate_cell(class cell * newme, int newtf, int newof);

	class cell * me;
	int times_flagged;
	int outof; // TODO: remove this after I verify its useless

	float risk(); // TODO: after outof is removed, need to delete this?
};


// holds the list of all allocations found, as a struct for some extra utility
// to combine the results of recursion on separate chains, average the results from each chain and sum them (pending any better idea)
struct podwise_return {
	podwise_return();
	podwise_return(float ans, int howmany);

	std::list<struct solutionobj> solutions; // list of all allocations found; unsorted, includes many duplicates
	int effort; // represents # of solutions found, preserved even after .avg function
	std::list<struct aggregate_cell> agg_info;
	int agg_allocs;

	inline struct podwise_return& operator+=( class cell * & addlist);
	inline struct podwise_return& operator+=(const std::vector<class cell *>& addvect);
	inline struct podwise_return& operator+=(const std::list<class cell *>& addlist);
	inline struct podwise_return& operator*=(const int& rhs);
	inline struct podwise_return& operator+=(const int& rhs);
	inline int size();
	inline float efficiency();
	float avg();
	//struct solutionobj * prmax();
	//struct solutionobj * prmin();
	float max_val();
	float min_val();
	int total_alloc(); // TODO: if 'perfectmode' aggregate data exists, use the size of one of them instead???
	void add_aggregate(class cell * newcell, int times_flagged, int outof);
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
private:
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
private:
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
private:
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






// small but essential utility functions
inline int compare_list_of_cells(std::list<class cell *> a, std::list<class cell *> b);
bool sort_scenario_blind(std::list<struct link>::iterator a, std::list<struct link>::iterator b);
inline int compare_list_of_scenarios(std::list<std::list<struct link>::iterator> a, std::list<std::list<struct link>::iterator> b);
bool sort_list_of_scenarios(std::list<std::list<struct link>::iterator> a, std::list<std::list<struct link>::iterator> b);
bool equivalent_list_of_scenarios(std::list<std::list<struct link>::iterator> a, std::list<std::list<struct link>::iterator> b);
inline int compare_aggregate_cell(struct aggregate_cell a, struct aggregate_cell b);
bool sort_aggregate_cell(struct aggregate_cell a, struct aggregate_cell b);
bool equivalent_aggregate_cell(struct aggregate_cell a, struct aggregate_cell b);

std::list<std::vector<int>> comb(int K, int N);
inline int comb_int(int K, int N);
inline int factorial(int x);




// functions for the recursive smartguess method(s)
int strat_multicell_logic_and_chain_builder(struct chain * buildme, int * thingsdone);
int strat_endsolver_and_chain_reducer_logic(std::vector<struct podwise_return> * prvect, std::list<class cell *> * interior_list, int * thingsdone);
int smartguess(struct chain * master_chain, struct game_stats * gstats, int * thingsdone);
struct podwise_return podwise_recurse(int rescan_counter, int mines_from_above, struct chain * mychain, bool use_endsolver);



// single-cell logic
int strat_singlecell(class cell * me, int * thingsdone);


// two-cell strategies
int strat_121_cross(class cell * center, struct game_stats * gstats, int * thingsdone);
int strat_nonoverlap_flag(class cell * center, struct game_stats * gstats, int * thingsdone);
int strat_nonoverlap_safe(class cell * center, struct game_stats * gstats, int * thingsdone);

//int strat_121_cross_Q(class cell * center, struct game_stats * gstats, std::list<class cell *> * clearlist);
//int strat_nonoverlap_flag_Q(class cell * center, struct game_stats * gstats, std::list<class cell *> * flaglist);
//int strat_nonoverlap_safe_Q(class cell * center, struct game_stats * gstats, std::list<class cell *> * clearlist);






#endif