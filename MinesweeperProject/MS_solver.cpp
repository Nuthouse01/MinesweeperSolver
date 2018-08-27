// Brian Henson 7/4/2018
// this file contains the solver code/logic as implemented by whoever is writing it





#include "MS_settings.h"
#include "MS_basegame.h"
#include "MS_stats.h"

#include "MS_solver.h" // include myself

// a global flag used in the recursive smartguess
bool recursion_safety_valve = false; // if recursion goes down a rabbithole, start taking shortcuts (not needed outside this file)


// **************************************************************************************
// member functions for the buttload of structs

solutionobj::solutionobj(float ans, int howmany) {
	answer = ans; allocs_encompassed = howmany;
	allocation.clear(); // init as empty list
}
aggregate_cell::aggregate_cell() {
	me = NULL; times_flagged = 0;
}
aggregate_cell::aggregate_cell(class cell * newme, int newtf) {
	me = newme; times_flagged = newtf;
}

podwise_return::podwise_return() { // construct empty
	solutions.clear();
	agg_info.clear();
	effort = 0;
	agg_allocs = 0;
}
podwise_return::podwise_return(float ans, int howmany) { // construct from one value, signifies end-of-branch
	solutions = std::list<struct solutionobj>(1, solutionobj(ans, howmany));
	agg_info.clear();
	effort = 1;
	agg_allocs = 0;
}
// obvious
inline int podwise_return::size() { return solutions.size(); }
// returns value 0-1 showing what % of allocations were NOT tried (because they were found redundant), therefore higher efficiency is better
inline float podwise_return::efficiency() {
	float t1 = float(effort) / float(total_alloc());
	float t2 = 1. - t1;
	if (t2 > 0.) { return t2; } else { return 0.; }
}
// podwise_return += cell pointer: append into all solutions
inline struct podwise_return& podwise_return::operator+=( class cell * & addcell) {
	for (std::list<struct solutionobj>::iterator listit = solutions.begin(); listit != solutions.end(); listit++) {
		listit->allocation.push_back(addcell);
	}
	return *this;
}
// podwise_return += VECTOR of cell pointers: append into all solutions
inline struct podwise_return& podwise_return::operator+=(const std::vector<class cell *>& addvect) {
	for (std::list<struct solutionobj>::iterator listit = solutions.begin(); listit != solutions.end(); listit++) {
		listit->allocation.insert(listit->allocation.end(), addvect.begin(), addvect.end());
	}
	return *this;
}
// podwise_return += list of cell pointers: append into all solutions
inline struct podwise_return& podwise_return::operator+=(const std::list<class cell *>& addlist) {
	for (std::list<struct solutionobj>::iterator listit = solutions.begin(); listit != solutions.end(); listit++) {
		listit->allocation.insert(listit->allocation.end(), addlist.begin(), addlist.end());
	}
	return *this;
}
// podwise_return *= int: multiply into all 'allocs_encompassed' in the list
inline struct podwise_return& podwise_return::operator*=(const int& rhs) {
	for (std::list<struct solutionobj>::iterator listit = solutions.begin(); listit != solutions.end(); listit++) { listit->allocs_encompassed *= rhs; }
	// multiply into the values of each aggregate_cell object
	agg_allocs *= rhs;
	for (std::list<struct aggregate_cell>::iterator aggit = agg_info.begin(); aggit != agg_info.end(); aggit++) { 
		aggit->times_flagged *= rhs;
	}
	return *this;
}
// podwise_return += int: increment all 'answers' in the list
inline struct podwise_return& podwise_return::operator+=(const int& rhs) {
	for (std::list<struct solutionobj>::iterator listit = solutions.begin(); listit != solutions.end(); listit++) { listit->answer += rhs; }
	return *this;
}
// return the WEIGHTED average of all the contents that are <= mines_remaining
// also modifies the object by deleting any solutions with answers > mines_remaining
float podwise_return::avg() {
	float a = 0; int total_weight = 0;
	std::list<struct solutionobj>::iterator listit = solutions.begin();
	while (listit != solutions.end()) {
		if (listit->answer <= float(mygame.get_mines_remaining())) {
			if (USE_WEIGHTED_AVG) {
				a += listit->answer * listit->allocs_encompassed;
				total_weight += listit->allocs_encompassed;
			} else {
				a += listit->answer;
				total_weight++;
			}
			listit++;
		} else {
			// erase listit
			myprintfn(2, "well i guess i didn't fix that problem :(\n");
			listit = solutions.erase(listit); // erase and advance
		}
	}
	if (total_weight == 0) {
		// this should be impossible
		myprintfn(2, "ERR: IN AVG, ALL SCENARIOS FOUND ARE TOO BIG\n"); assert(0); return 0.;
	}
	return (a / float(total_weight));
}
// max_val: find the max 'answer' value from all the solutions in the PR object
float podwise_return::max_val() {
	assert(!solutions.empty());
	float retmax = 0.;
	for (std::list<struct solutionobj>::iterator solit = solutions.begin(); solit != solutions.end(); solit++) {
		if (solit->answer <= retmax) { continue; } // existing max is greater
		if (solit->answer > retmax) { retmax = solit->answer; }
	}
	return retmax;
}
// min_val: find the min 'answer' value from all the solutions in the PR object
float podwise_return::min_val() {
	assert(!solutions.empty());
	float retmin = 100000000.;
	for (std::list<struct solutionobj>::iterator solit = solutions.begin(); solit != solutions.end(); solit++) {
		if (solit->answer >= retmin) { continue; }// existing max is greater
		if (solit->answer < retmin) { retmin = solit->answer; }
	}
	return retmin;
}
// total_alloc: sum the alloc values for all solutions in the list
int podwise_return::total_alloc() {
	int retval = 0;
	for (std::list<struct solutionobj>::iterator solit = solutions.begin(); solit != solutions.end(); solit++) { retval += solit->allocs_encompassed; }
	return retval;
}
// search agg_info for the given cell; if there, inc the data. if not, add it in sorted order
void podwise_return::add_aggregate(class cell * newcell, float times_flagged) {
	// use the compare function to step through the list until i find the existing cell (or where it should be)
	int r = 0;
	for (std::list<struct aggregate_cell>::iterator pos = agg_info.begin(); pos != agg_info.end(); pos++) {
		r = compare_two_cells(newcell, pos->me); // if a goes first, return negative
		if (r < 0) {// insert
			agg_info.insert(pos, aggregate_cell(newcell, times_flagged * agg_allocs)); return;
		}
		if (r == 0) {// combine, should never happen?
			pos->times_flagged += times_flagged * agg_allocs;
			return;
		}
	}
	// if it falls out of the loop, then either list is empty or newcell should be added to end
	agg_info.push_back(aggregate_cell(newcell, times_flagged * agg_allocs));
}



// shortcut to calculate the risk for a pod
float pod::risk() { return (100. * float(mines) / float(size())); }
// if outside recursion, return cell_list.size(); if inside recursion, return cell_list_size
int pod::size() { if (cell_list_size < 0) { return cell_list.size(); } else { return cell_list_size; } }
// scan the pod for any "special cases", perform 'switch' on result: 4=disjoint, 1=negative risk, 2=0 risk, 3=100 risk, 0=normal
int pod::scan() {
	if (links.empty()) { return 4; }
	float z = risk(); // might crash if it tries to divide by size 0, so disjoint-check must be first
	if (z < 0.) { return 1; }
	if (z == 0.) { return 2; }
	if (z == 100.) { return 3; }
	return 0;
}
// init from root, mines from root, link_cells empty, cell_list from root
pod::pod(class cell * new_root) {
	root = new_root;
	mines = new_root->get_effective(); // the root is visible, so this is fine
	links = std::list<struct link>(); // links initialized empty
	cell_list = mygame.filter_adjacent(new_root, UNKNOWN); // find adjacent unknowns
	chain_idx = -1; // set later
	cell_list_size = -1; // set later
}
// add a link object to my list of links; doesn't modify cell_list, doesn't modify any other pod
// inserts it into sorted position, so the list of shared_roots will always be sorted
void pod::add_link(class cell * shared, class cell * shared_root) {
	// search for link... if found, add new root. if not found, create new.
	std::list<struct link>::iterator link_iter;
	for (link_iter = links.begin(); link_iter != links.end(); link_iter++) {
		if (link_iter->link_cell == shared) {
			// if a match is found, add it to existing
			link_iter->linked_roots.push_back(shared_root);
			std::list<class cell *>::iterator middle = link_iter->linked_roots.end(); middle--;
			//if (link_iter->linked_roots.begin() != middle) {
				// note: for inplace_merge to work, begin != middle and middle != end
				// but because this only merges if existing thing is found, this check is not needed
				std::inplace_merge(link_iter->linked_roots.begin(), middle, link_iter->linked_roots.end(), sort_by_position);
			//}
			break;
		}
	}
	if (link_iter == links.end()) {
		// if match is not found, then create new
		links.push_back(link(shared, shared_root));
	}
}
// remove the link from this pod. return 0 if it worked, 1 if the link wasn't found
// if the link in question is a flag, also reduces the 'mines' member
int pod::remove_link(class cell * shared, bool isaflag) {
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
				if (i == s) { myprintfn(2, "ERR: CAN'T FIND LINK TO REMOVE IN CELL_LIST\n"); } // debug
			}
			if (isaflag) { mines--; }	// only sometimes reduce flags
			links.erase(link_iter);
			return 0;
		}
	}
	// note: won't always find the link, in that case just 'continue'
	return 1;
}
// find all possible ways to allocate mines in the links/internals that aren't duplicate or equivalent
std::list<struct scenario> pod::find_scenarios(bool use_t2_opt) {
	// return a list of scenarios,
	// where each scenario is a list of ITERATORS which point to my LINKS
	std::list<struct scenario> retme;
	// determine the range of values K to use, how many cells to pick out of the links (because there will be variable mines in the non-link cells)
	int t = (mines - (size() - links.size()));
	int start = max(0, t); // shouldn't be negative
	int end = min(mines, links.size()); // lesser of mines/links

	for (int i = start; i <= end; i++) {
		// FIRST-TIER optimization: ignore actual allocation in non-link cells, just find all ways to put flags on link cells
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
			// sort the scenario blind-wise (ignoring the actual link cell, only looking at the pods it connects to)
			// this way equiv links (links connected to the same pods) are sequential and can be easily detected
			if (use_t2_opt) { buildme.back().sort(sort_scenario_blind); }
		}

		// now 'ret' has been converted to 'buildme', list of sorted lists of link-list-iterators
		if (use_t2_opt) { buildme.sort(sort_list_of_scenarios); }
		// SECOND-TIER optimization: eliminate scenarios containing the same number of inter-equivalent links
		// now I have to do my own uniquify-and-also-build-scenario-objects section
		// but whenever I find a duplicate I combine their allocation numbers and only use one
		std::list<struct scenario> retme_sub;
		std::list<std::list<std::list<struct link>::iterator>>::iterator buildit = buildme.begin();
		std::list<std::list<std::list<struct link>::iterator>>::iterator previt = buildit;
		int c = comb_int((mines - i), (size() - links.size())); // how many ways are there to allocate mines among the non-link cells?
		retme_sub.push_back(scenario((*buildit), c));
		buildit++; // buildit starts at 1, previt starts at 0
		while (buildit != buildme.end()) {
			if (use_t2_opt && (compare_list_of_scenarios(*previt, *buildit) == 0)) {
				// if they are the same, revise the previous entry
				retme_sub.back().allocs_encompassed += c;
			} else {
				// if they are different, add as a new entry
				retme_sub.push_back(scenario((*buildit), c));
			}
			previt++; buildit++;
		}
		// add retme_sub to the total string retme
		retme.insert(retme.end(), retme_sub.begin(), retme_sub.end());
	}
	return retme;
}
// when given an int, retrieve the link at that index
std::list<struct link>::iterator pod::get_link(int l) {
	std::list<struct link>::iterator link_iter = links.begin();
	for (int i = 0; i < l; i++) { link_iter++; }
	return link_iter;
}


// construct from the cell being shared and the root of the other guy
link::link(class cell * shared, class cell * shared_root) {
	link_cell = shared;
	linked_roots.push_back(shared_root);
}
// two links are equivalent if they have the same shared_cell and also their shared_roots compare equal, needed for remove-by-value
inline bool link::operator== (const struct link o) const {
	// if they are the same size...
	if ((o.link_cell != link_cell) || (linked_roots.size() != o.linked_roots.size())) { return false; }
	std::list<class cell *>::const_iterator ait = linked_roots.begin();
	std::list<class cell *>::const_iterator bit = o.linked_roots.begin();
	// ... and if they have the same contents...
	while (ait != linked_roots.end()) {
		if (*ait != *bit) { return false; }
		ait++; bit++;
	}
	// ... then i guess they're identical
	return true;
}


// constructor
chain::chain() {
	podlist = std::list<struct pod>(); // explicitly init as an empty list
}
// find a pod with the given root, return iterator so it's easier to delete or whatever
std::list<struct pod>::iterator chain::root_to_pod(class cell * linked_root) {
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

// returns a VECTOR of POD-LIST ITERATORS to the pods in the podlist that are within 5x5 of the given pod
// if include_corners=false, also ignore the corners of the 5x5
std::vector<std::list<struct pod>::iterator> chain::get_5x5_around(std::list<struct pod>::iterator center, bool include_corners) {
	std::vector<std::list<struct pod>::iterator> retme;
	for (int b = -2; b < 3; b++) { for (int a = -2; a < 3; a++) { // iterate over 5x5
		if ((!include_corners && ((a == -2 || a == 2) && (b == -2 || b == 2))) || (a == 0 && b == 0)) { continue; } // skip myself and also the corners
		class cell * other = mygame.cellptr(center->root->x + a, center->root->y + b);
		if ((other == NULL) || (other->get_status() != VISIBLE)) { continue; } // if the cell doesn't exist or isn't VISIBLE, then skip
		std::list<struct pod>::iterator otherpod = this->root_to_pod(other);// find the pod in the chain with this cell as root!
		if (otherpod != this->podlist.end()) { // if 'other' has a corresponding pod, then add to the list
			retme.push_back(otherpod); // need to de-iterator it, then turn it into an ordinary pointer
		}
	}}
	return retme;
}

// after calling identify_chains, sort the pods into a vector of chains
// if 'reduce' is true, then also turn "cell_list" into a simple number, and clear the actual list (only for before recursion!)
std::vector<struct chain> chain::sort_into_chains(int r, bool reduce) {
	std::vector<struct chain> chain_list; // where i'll be sorting them into
	std::vector<class cell *> blanklist = std::vector<class cell *>();
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
void chain::identify_chains_recurse(int idx, struct pod * me) {
	if (me->chain_idx != -1) {
		assert(me->chain_idx == idx); // if its not -1 then it damn well better be idx
		return; // this pod already marked by something (hopefully this same recursion call)
	}
	me->chain_idx = idx; // paint it
	for (std::list<struct link>::iterator linkit = me->links.begin(); linkit != me->links.end(); linkit++) {
		// for each link object 'linkit' in me...
		for (std::list<class cell *>::iterator rootit = linkit->linked_roots.begin(); rootit != linkit->linked_roots.end(); rootit++) {
			// for each root pointer 'rootit' in linkit...
			// ...get the pod that is linked...
			struct pod * linked_pod = &(*(root_to_pod(*rootit)));
			// ...and recurse on that pod with the same index
			identify_chains_recurse(idx, linked_pod);
		}
	}
	return;
}


// constructor
riskholder::riskholder(int x, int y) {
	std::vector<float> asdf2;	// needed for using the 'resize' command
	asdf2.reserve(4);			// the max size needed is 8 but that's unlikely, lets just start with 4
	std::vector<std::vector<float>> asdf;	// init empty
	asdf.resize(y, asdf2);					// fill it with copies of asdf2
	asdf.shrink_to_fit();					// set capacity exactly where i want it, no bigger
	riskarray.clear();			// init empty
	riskarray.resize(x, asdf);	// fill it with copies of asdf
	riskarray.shrink_to_fit();	// set capacity exactly where i want it, no bigger
}
// takes a cell pointer and adds a risk to its list... could modify to use x/y, but why bother?
void riskholder::addrisk(class cell * foo, float newrisk) {
	(riskarray[foo->x][foo->y]).push_back(newrisk);
}
// iterate over itself and return the stuff tied for lowest risk
std::pair<float, std::list<class cell *>> riskholder::findminrisk() {
	std::list<class cell *> minlist;
	float minrisk = 100.;
	for (int m = 0; m < myruninfo.get_SIZEX(); m++) {
		for (int n = 0; n < myruninfo.get_SIZEY(); n++) {
			float j = finalrisk(m, n);
			if ((j == -1.) || (j > minrisk))
				continue;
			if (j < minrisk) {
				minrisk = j;
				minlist.clear();
			}
			minlist.push_back(&mygame.field[m][n]);
		}
	}
	std::pair<float, std::list<class cell *>> retme(minrisk, minlist);
	return retme;
}
// find the avg/max/min of the risks and return that value; also clear the list. if list is already empty, return -1
float riskholder::finalrisk(int x, int y) {
	if ((riskarray[x][y]).empty())
		return -1.;
	float retval = 0.;
	int s = (riskarray[x][y]).size();
	if (s == 1) { 
		retval = riskarray[x][y][0]; 
	} else if (RISK_CALC_METHOD == 0) { // AVERAGE
		float sum = 0.;
		for (int i = 0; i < s; i++) {
			sum += riskarray[x][y][i];
		}
		retval = sum / float(s);
	} else if (RISK_CALC_METHOD == 1) { // MAXIMUM
		retval = -1.;
		for (int i = 0; i < s; i++) {
			float t = riskarray[x][y][i];
			if (t > retval) { retval = t; }
		}
	}
	(riskarray[x][y]).clear(); // clear it for use next time
	return retval;
}


// basic tuple-like constructor
scenario::scenario(std::list<std::list<struct link>::iterator> map, int num) {
	scenario_links_to_flag = map; allocs_encompassed = num;
}






// **************************************************************************************
// sort/uniquify functions and general-use utility functions

// if a goes first, return negative; if b goes first, return positive; if identical, return 0
// compare lists of cells
// compare_list_of_cells -> compare_two_cells
inline int compare_list_of_cells(std::list<class cell *> a, std::list<class cell *> b) {
	if (a.size() != b.size())
		return b.size() - a.size();
	// if they are the same size...
	std::list<class cell *>::iterator ait = a.begin();
	std::list<class cell *>::iterator bit = b.begin();
	while (ait != a.end()) {
		int c = compare_two_cells(*ait, *bit);
		if (c != 0) { return c; }
		ait++; bit++;
	}
	return 0; // i guess they're identical
}
// return true if the 1st arg goes before the 2nd arg, return false if 1==2 or 2 goes before 1
// compare scenarios (lists of cells), while ignoring the shared_cell arg
// sort_scenario_blind -> compare_list_of_cells -> compare_two_cells
bool sort_scenario_blind(std::list<struct link>::iterator a, std::list<struct link>::iterator b) {
	return (compare_list_of_cells(a->linked_roots, b->linked_roots) < 0);
}
// if a goes first, return negative; if b goes first, return positive; if identical, return 0
// compare lists of scenarios (lists of lists of cells), while ignoring the shared_cell arg
// compare_list_of_scenarios -> compare_list_of_cells -> compare_two_cells
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
	return 0; // i guess they're identical
}
// return true if the 1st arg goes before the 2nd arg, return false if 1==2 or 2 goes before 1
// intentionally ignores the shared_cell member
// sort_list_of_scenarios -> compare_list_of_scenarios -> compare_list_of_cells -> compare_two_cells
bool sort_list_of_scenarios(std::list<std::list<struct link>::iterator> a, std::list<std::list<struct link>::iterator> b) {
	return (compare_list_of_scenarios(a, b) < 0);
}
// equivalent_list_of_scenarios -> compare_list_of_scenarios -> compare_list_of_cells -> compare_two_cells
bool equivalent_list_of_scenarios(std::list<std::list<struct link>::iterator> a, std::list<std::list<struct link>::iterator> b) {
	return (compare_list_of_scenarios(a, b) == 0);
}
inline int compare_aggregate_cell(struct aggregate_cell a, struct aggregate_cell b) {return compare_two_cells(a.me, b.me);}
bool sort_aggregate_cell(struct aggregate_cell a, struct aggregate_cell b) {return (compare_aggregate_cell(a, b) < 0);}
bool equivalent_aggregate_cell(struct aggregate_cell a, struct aggregate_cell b) {return (compare_aggregate_cell(a, b) == 0);}



// determine all ways to choose K from N, return a VECTOR of VECTORS of INTS
// result is a list of lists of indices from 0 to N-1
// ex: comb(3,5)
std::list<std::vector<int>> comb(int K, int N) {
	assert(K <= N);
	// thanks to some dude on StackExchange
	std::list<std::vector<int>> buildme;
	if (K == 0 || N == 0) {
		// even if choosing from 0, I should return one solution, even if it is an empty solution
		return std::list<std::vector<int>>(1, std::vector<int>());
	}
	std::vector<bool> bitmask = std::vector<bool>();
	bitmask.resize(K, 1);	// a vector of K leading 1's...
	bitmask.resize(N, 0);	// ... followed by N-K trailing 0's, total length = N
							// permutate the string of 1s and 0s and see what happens
	do {
		buildme.push_back(std::vector<int>(K, -1)); // start a new entry, know it will have size K
		int z = 0;
		//buildme->back().reserve(K); // either just the same efficiency, or slightly better
		for (int i = 0; i < N; ++i) {// for each index, is the bit at that index a 1? if yes, choose that item
			if (bitmask[i]) { buildme.back()[z] = i; z++; }
		}
	} while (std::prev_permutation(bitmask.begin(), bitmask.end()));
	//bitmask.clear(); //hopefully doesn't reduce capacity, that would undermine the whole point of static memory!
	return buildme;
}

// return the # of total combinations you can choose K from N, simple math function
// = N! / (K! * (N-K)!)
inline int comb_int(int K, int N) {
	assert(K <= N);
	return factorial(N) / (factorial(K) * factorial(N - K));
}
// i never need to know higher than 8! so just hardcode the answers
inline int factorial(int x) {
	if (x < 2) return 1;
	switch (x) {
		case 2: return 2; case 3: return 6; case 4: return 24; case 5: return 120; 
		case 6: return 720; case 7: return 5040; case 8: return 40320; default: return -1;
	}
}


// **************************************************************************************
// smartguess functions
/*
ROADMAP: the 11-step plan to total victory! (does not include numbered sub-steps of podwise_recurse or endsolver)
smartguess {
	strat_multicell_logic_and_chain_builder {
		1)build the pods from visible, allow for dupes, no link cells yet. is stored in the "master chain".
		2)iterate over pods, check for dupes and subsets, apply *multicell logic* (call extract_overlap on each pod with root in 5x5 around my root)
		note if a pod becomes 100% or 0%, loop until no changes happen
		3)if any pods became 100% or 0%, clear/flag those and return (don't continue on to rest of smartguess)
	}
	4)iterate again, removing pod contents from 'interior_list'. Done here so there are fewer dupe pods, less time searching
	if (!interior_list.empty() || use_endsolver || use_smartguess) {
		5)iterate again, building links to anything within 5x5 that shares contents
		6)iterate/recurse, breaking master chain into multiple interlinked islands/chains... may also wipe cell_list to save memory while recursing
		7)for each chain, recurse!!!!!!!!!!!!!!!!!!!!!!!!!
		podwise_recurse {
			0: are we done? look for an excuse to return
			1: chain rescan section (DEPRECATED)
			2: pick a pod, find all ways to saturate it, termed 'scenarios'. Then, for each scenario,
				3a: apply the changes according to the scenario by moving links from chosen pod to links_to_flag_or_clear
				3b: until links_to_flag_or_clear is empty, iterate! removing the links from any pods they appear in...
				3c: after each change, see if activepod has become 100/0/disjoint/negative... if so, add its links to links_to_flag_or_clear as flag or clear
				3d: after dealing with everything the link is connected to, deal with the actual link cell itself
				4: recurse! when it returns, combine the 'lower' info with 'thislvl' info (VERTICAL combining)
				5: combine the 'lower' info into 'retval', (HORIZONTAL combining)
			6: return 'retval', the combined answer found from of all recursion below this point
		}
		8)run the endsolver logic if appropriate, if something is found to be definitely flag/clear then apply them and return from smartguess
		strat_endsolver_and_solution_reducer_logic {
			1: check if any solutions' maximum value is too large and cannot be compensated for by the minimums of all other solutions
			2: check if any solutions' minimum value is too small and cannot be compensated for by the maximums of all other solutions + interior_list
			3: check if the puzzle is solved by using maximum answers for all solutions + fully mining interior_list
			4: check if the puzzle is solved by using minimum answers for all solutions
		}
		9)calculate the interior_risk and check how accurate my guess is
	}
	10)calculate the minimum risk of any border cells and find those cells (derived from simple pod-risk, or from aggregate data, or from comprehensive list of all possibe allocations)
	if something is found to be definitely flag/clear then apply them and return from smartguess
	11)decide between border and interior, call 'rand_from_list', clear the chosen cell, and return
}
*/



// build the master chain that will be used for smartguess... encapsulates steps 1/2/3 of original 11-stage plan
// also apply "multicell" versions of nonoverlap-safe and nonoverlap-flag; partial matches and chaining logic rules together
// may identify some cells as "definite safe" or "definite mine", and will reveal/flag them internally
// if no cells are flagged/cleared, will output the master chain thru input arg for passing to full recursive function below
// return: 1=win/-1=loss/0=continue (winning is rare but theoretically possible, but cannot lose unless something is seriously out of whack)
int strat_multicell_logic_and_chain_builder(struct chain * buildme, int * thingsdone) {
	static class cell dummycell; // cell w/ value 0
	static struct pod dummypod; // pod w/ cell_list empty
	static std::list<struct pod> dummylist; // one-entry list for dummypod
	static std::list<struct pod>::iterator dummyitr; // iterator to beginning of dummylist
	if (dummylist.empty()) { // one-time init for these static things
		dummypod.root = &dummycell; // can't use constructor cuz constructor automatically fills the cell_list
		dummylist.resize(1, dummypod);
		dummyitr = dummylist.begin(); // must have an iterator to the dummy, but it doesn't need to be in the same list as the others
	}

	std::list<class cell *> clearme;
	std::list<class cell *> flagme;

	// step 1: iterate over field, get 'visible' cells, use them as roots to build pods and build the chain.
	// chain is non-optimized: includes duplicates, before pod-subtraction.
	// pods are added to the chain already sorted (reading order by root), as are their cell_list contents.
	for (int y = 0; y < myruninfo.get_SIZEY(); y++) {
		for (int x = 0; x < myruninfo.get_SIZEX(); x++) { // iterate over each cell
			if (mygame.field[x][y].get_status() == VISIBLE) {
				buildme->podlist.push_back(pod(&mygame.field[x][y])); // constructor gets adj unks for the given root
			}
		}
	}

	// step 2: iterate over pods, check for dupes and subsets (call extract_overlap on each pod with root in 5x5 around my root)
	// this is where the "multicell logic" comes into play, NOV-SAFE and NOV-FLAG partial matches chained together many times
	// note if a pod becomes 100% or 0%... repeat until no changes are done. when deleting pods, what remains will still be in sorted order

	/*
	for each pod,
		get 5x5 around, otherpod (VECTOR of POD-LIST ITERATORS to the pods in podlist)
		for each otherpod in around,
			check if pod==otherpod (dupe)
			check if pod<otherpod (subset)
			for each secondpod in around (starting secondpod==otherpod)
				if secondpod==otherpod, instead use 0,0 pod and disable NOV-SAFEx3 section
				*** begin NOV-FLAGx3 and NOV-SAFEx3 logic ***
	*/

	bool changes = false;
	bool erased_myself = false;
	do {
		changes = false;
		for (std::list<struct pod>::iterator podit = buildme->podlist.begin(); podit != buildme->podlist.end(); ) { // intentionally missing post-loop incremnt
			std::vector<std::list<struct pod>::iterator> around = buildme->get_5x5_around(podit, false);
			for (int b = 0; b < around.size(); b++) {
				std::list<struct pod>::iterator otherpod = around[b]; // for each pod 'otherpod' with root within 5x5 found...

				std::vector<std::vector<class cell *>> N = extract_overlap(podit->cell_list, otherpod->cell_list);

				if (N[0].empty() && N[1].empty()) { // means podit == otherpod
					// if total duplicate, delete OTHER (not me)
					changes = true;
					buildme->podlist.erase(otherpod); // don't even need to rebuild 'around' vector!
				} else if (N[0].empty()) {			// means podit < otherpod
					// if podit has no uniques but otherpod does, then podit is subset of otherpod...
					changes = true;
					otherpod->cell_list = N[1]; // reduce otherpod to only its uniques
					otherpod->mines -= podit->mines; // subtract podit from otherpod
					// if a pod would become 100 or 0, otherpod would do it here.
					float z = otherpod->risk();
					if (z == 0.) {
						// add all cells in otherpod to clearme
						clearme.insert(clearme.end(), otherpod->cell_list.begin(), otherpod->cell_list.end());
					} else if (z == 100.) {
						// add all cells in otherpod to flagme
						flagme.insert(flagme.end(), otherpod->cell_list.begin(), otherpod->cell_list.end());
					}
				} else {
					// begin the NOV-FLAGx2, NOV-FLAGx3, and NOV-SAFEx3 section!
					// there are 5 stages of checks to know if FLAGx3 or SAFEx3 can be applied... A=podit, B1=otherpod, B2=secondpod
					// decided to use "if not true, continue" syntax cuz I didn't want to indent 5 layers of nested if-statements
					for (int s = b; s < around.size(); s++) { // start from b cuz only need all combos, not both orders of each combo
						std::list<struct pod>::iterator secondpod;
						if (s == b) {
							secondpod = dummyitr; // NOV-FLAGx3 special case to check NOV-FLAGx2, use pod with value=0 and size=0
						} else {
							secondpod = around[s]; // normal case
							// during normal case, NOV-FLAGx3 and NOV-SAFEx3 require minimum size = 4 and value = 2
							if (podit->cell_list.size() < 4 || podit->mines < 2) { break; } 
						}
						// 1) A >= B1 + B2: if greater than, NOV-FLAG. if equal to, NOV-SAFE. 
						// note: changed to instead be Z = A - B1 - B2, then Z > 0 or Z == 0. Equivalent logic, saves me some math later
						int Z = (podit->mines - otherpod->mines) - secondpod->mines;
						bool safemode = false; // must set flags because which passed makes a difference later on.
						if ((Z == 0) && (s != b)) { safemode = true; } // do not try NOV-SAFEx3 during 0,0 special case
						if (!(Z > 0 || safemode)) { continue; } // if neither is true, skip
						// 2) |overlap(A,B1)| > B1
						if (!(N[2].size() > otherpod->mines)) { continue; }
						// 3) |overlap(A,B2)| > B2
						// delay second call to extract_overlap as long as possible, so it might be skipped
						// vector N holds comparison between A and B1, vector U holds comparison between A and B2
						std::vector<std::vector<class cell *>> U = extract_overlap(podit->cell_list, secondpod->cell_list);
						if (!(U[2].size() > otherpod->mines)) { continue; }
						// 4) size(A) - |overlap(A,B1)| - |overlap(A,B2)| == Z, works whether Z is 0 or positive
						if (!(((podit->cell_list.size() - N[2].size()) - U[2].size()) == Z)) { continue; }
						// 5) |overlap( overlap(A,B1) , overlap(A,B2) )| == 0, AKA both overlapping sections must not overlap eachother
						// delay third call to extract_overlap as long as possible, too, so it might be skipped
						std::vector<std::vector<class cell *>> V = extract_overlap(N[2], U[2]);
						if (!(V[2].size() == 0)) { continue; }
						//////////////////////////////////////////////////////////////////
						// if ALL of these conditions are met, then we can FINALLY apply the operations!!

						// common operations:
						changes = true;
						// clear all O-unique cells and reduce O to its overlap area with podit
						clearme.insert(clearme.end(), N[1].begin(), N[1].end());
						otherpod->cell_list = N[2];

						if (safemode) {
							// NOV-SAFEx3: if safemode=true, then podit is completely covered by O+S, though they do spill outside podit
							// clear all S-unique cells and reduce S to its overlap area with podit
							clearme.insert(clearme.end(), U[1].begin(), U[1].end());
							secondpod->cell_list = U[2];
							// then erase podit! note: erasing podit (ME) isn't so simple, cuz then it won't be able to inc the iterator...
							podit = buildme->podlist.erase(podit); erased_myself = true;
							goto LABEL_CONTINUE_PODIT_LOOP;
						} else {
							// NOV-FLAGx3: if flagmode=true, then O+S cover all of podit except for Z cells. these cells are guaranteed flags.
							// NOV-FLAGx2: don't touch S at all unless the 0,0 dummy pod wasnt used
							if (s != b) {
								// reduce S to its overlap area with podit, and clear all S-unique cells
								clearme.insert(clearme.end(), U[1].begin(), U[1].end());
								secondpod->cell_list = U[2];
							}
							// then reduce podit to only these Z cells and flag them! no erase
							std::vector<std::vector<class cell *>> temp = extract_overlap(podit->cell_list, N[2]); // subtract O from podit
							std::vector<std::vector<class cell *>> uniq = extract_overlap(temp[0], U[2]); // subtract S from podit
							assert(uniq[0].size() == Z);
							podit->cell_list = uniq[0];
							podit->mines = Z;
							flagme.insert(flagme.end(), uniq[0].begin(), uniq[0].end());
							goto LABEL_CONTINUE_PODIT_LOOP;
						}
					}
				}
			}
			// erase returns the iterator to the one after the one it erased (or .end())
			// i need to jump to the next iteration of podit loop, tho
			// therefore whenever I erase podit, don't increment podit on that iteration of the loop
		LABEL_CONTINUE_PODIT_LOOP:
			if (erased_myself) { erased_myself = false; } else { podit++; } // implement my own ++ instead of in the for-loop
		} // end for each pod
	} while (changes);

	// NOTE: turns out that you can't safely apply 121 logic to the chain

	// step 3: clear the clearme and flag the flagme
	if (clearme.size() || flagme.size()) {
		if (myruninfo.SCREEN == 3) myprintfn(2, "DEBUG: in multicell, found %i clear and %i flag\n", clearme.size(), flagme.size());
		for (std::list<class cell *>::iterator cit = clearme.begin(); cit != clearme.end(); cit++) { // clear-list
			int r = mygame.reveal(*cit);
			*thingsdone += bool(r);
			if (r == -1) {
				myprintfn(2, "ERR: Unexpected loss during multicell logic reveal, must investigate!!\n"); assert(0); return -2;
			}
		}
		for (std::list<class cell *>::iterator fit = flagme.begin(); fit != flagme.end(); fit++) { // flag-list
			int r = mygame.set_flag(*fit);
			*thingsdone += bool(r);
			if (r == -1) { return 1; } // game won!
		}
	}
	return 0;
}



// smartguess: searches for chain solutions with only one allocation (unique) to apply
// perfectmode: ^ plus, if non-unique solution is found, eliminate all other solutions to that chain
//				^ plus, searches for chain solutions that are definitely invalid and eliminates them
// return: 1=win/-1=loss/0=continue (cannot lose unless something is seriously out of whack)
// but, it only causes smartguess to return to the main play_game level if this function adds to *thingsdone
int strat_endsolver_and_solution_reducer_logic(std::vector<struct podwise_return> * prvect, std::list<class cell *> * interior_list, int * thingsdone) {
	/* ENDSOLVER uses the following logic:
	1: for chain X in retholder, if max(X)+min(others) > mines_remain, then:
		   anything in X with value max(X) is definitely an invalid solution. also, should check the next-biggest in X, and so on.
	2: for chain X in retholder, if min(X)+max(others)+size(int) < mines_remain, then:
		   anything in X with value min(X) is definitely an invalid solution. also, should check the next-smallest in X, and so on.
	if a max is eliminated, check the minimums again. if a min is eliminated, check the maximums again.
	at the end of this, if there is only 1 solution remaining, and it is unique, it can be applied!
	3: if max(all)+size(int) == mines_remain, then:
		   if any of these maximums are unique solutions, they can be applied. else, at least we know all non-maximum solutions are invalid
	4: if  min(all)+0 == mines_remain, then:
		   if any of these minimums are unique solutions, they can be applied. else, at least we know all non-minimum solutions are invalid
   */

	// NOTE: I want to keep track of how many solutions were elimiated out of how many total!
	int checkminelim = 0, checkmaxelim = 0, checkallminelim = 0, checkallmaxelim = 0;
	int num_sol_start = 0;
	for (std::vector<struct podwise_return>::iterator priter = prvect->begin(); priter != prvect->end(); priter++) {
		num_sol_start += priter->solutions.size();
	}
	int minesval = mygame.get_mines_remaining();

	bool checkminimums = true, checkmaximums = true; // flags
	do {
		if (checkmaximums) {
			checkmaximums = false;
			//for each priter in prvect
			for (std::vector<struct podwise_return>::iterator priter = prvect->begin(); priter != prvect->end(); priter++) {
				bool foundsomething = false;
				do {
					foundsomething = false;
					// get the max of this one
					int memax = priter->max_val();
					// get the mins of all others
					int othermin = 0;
					for (std::vector<struct podwise_return>::iterator priter2 = prvect->begin(); priter2 != prvect->end(); priter2++) {
						if (priter2 == priter) { continue; } else { othermin += priter2->min_val(); }
					}
					if ((memax + othermin) > minesval) {
						checkminimums = true; foundsomething = true;
						//from priter, delete all solutions with value maxval()
						//inc num_sol_eliminated accordingly
						std::list<struct solutionobj>::iterator solit = priter->solutions.begin();
						while (solit != priter->solutions.end()) {
							if (solit->answer == memax) {
								solit = priter->solutions.erase(solit); // delete and advance
								checkmaxelim++;
							} else { solit++; } // just advance
						}
					}
				} while (foundsomething); // if something matched and was removed, then check again
			}
		}
		if (checkminimums) {
			checkminimums = false;
			//for each priter in prvect
			for (std::vector<struct podwise_return>::iterator priter = prvect->begin(); priter != prvect->end(); priter++) {
				bool foundsomething = false;
				do {
					foundsomething = false;
					// get the min of this one
					int memin = priter->min_val();
					// get the maxes of all others
					int othermax = 0;
					for (std::vector<struct podwise_return>::iterator priter2 = prvect->begin(); priter2 != prvect->end(); priter2++) {
						if (priter2 == priter) { continue; } else { othermax += priter2->max_val(); }
					}
					if ((memin + othermax + interior_list->size()) < minesval) {
						checkmaximums = true; foundsomething = true;
						//from priter, delete all solutions with value minval()
						//inc num_sol_eliminated accordingly
						std::list<struct solutionobj>::iterator solit = priter->solutions.begin();
						while (solit != priter->solutions.end()) {
							if (solit->answer == memin) {
								solit = priter->solutions.erase(solit); // delete and advance
								checkminelim++;
							} else { solit++; } // just advance
						}
					}
				} while (foundsomething); // if something matched and was removed, then check again
			}
		}
	} while (checkminimums || checkmaximums);

	// if something was eliminated after this iterating,
	if (checkminelim || checkmaxelim) {
		// for each PR object,
		for (std::vector<struct podwise_return>::iterator priter = prvect->begin(); priter != prvect->end(); priter++) {
			// if this PR has only one solution remaining, and it is unique, that solution can be applied! unlikely but possible
			if (((priter->solutions.size()) == 1) && (priter->solutions.front().allocs_encompassed == 1)) {
				for (std::list<class cell *>::iterator celliter = priter->solutions.front().allocation.begin(); celliter != priter->solutions.front().allocation.end(); celliter++) {
					int r = mygame.set_flag(*celliter);
					*thingsdone += bool(r);
					if (r == -1) { return 1; } else if (r == 2) { assert(0); return -2; }
				}
			}
		}
	}

	// NOTE: if this successfully gets down to only 1 solution and applies it above, then it will guaranteed be picked up below as well

	float minsum = 0; float maxsum = 0;
	for (std::vector<struct podwise_return>::iterator priter = prvect->begin(); priter != prvect->end(); priter++) {
		minsum += priter->min_val(); maxsum += priter->max_val();
	}
	if (minsum == minesval) {
		// int_list is safe
		for (std::list<class cell *>::iterator iiter = interior_list->begin(); iiter != interior_list->end(); iiter++) {
			*thingsdone += 1;
			if (mygame.reveal(*iiter) == -1) {
				myprintfn(2, "ERR: Unexpected loss during smartguess chain-solve!!\n"); assert(0); return -2;
			}
		}
		// for each PR object, 
		for (std::vector<struct podwise_return>::iterator priter = prvect->begin(); priter != prvect->end(); priter++) {
			// find the minimum value
			int minval = priter->min_val();
			// delete any solutions that aren't that value, and inc num_sol_eliminated
			std::list<struct solutionobj>::iterator solit = priter->solutions.begin();
			while (solit != priter->solutions.end()) {
				if (solit->answer != minval) {
					solit = priter->solutions.erase(solit); // delete and advance
					checkallminelim++;
				} else { solit++; } // just advance
			}
			// if this PR has only one solution remaining, and it is unique, that solution can be applied! unlikely but possible
			if (((priter->solutions.size()) == 1) && (priter->solutions.front().allocs_encompassed == 1)) {
				for (std::list<class cell *>::iterator celliter = priter->solutions.front().allocation.begin(); celliter != priter->solutions.front().allocation.end(); celliter++) {
					int r = mygame.set_flag(*celliter);
					*thingsdone += bool(r);
					if (r == -1) { return 1; } else if (r == 2) { assert(0); return -2; }
				}
			}
		}
	} else if ((maxsum + interior_list->size()) == minesval) {
		// int_list is all mines
		for (std::list<class cell *>::iterator iiter = interior_list->begin(); iiter != interior_list->end(); iiter++) {
			int r = mygame.set_flag(*iiter);
			*thingsdone += bool(r);
			if (r == -1) { return 1; } else if (r == 2) { assert(0); return -2; }
		}
		// for each PR object, 
		for (std::vector<struct podwise_return>::iterator priter = prvect->begin(); priter != prvect->end(); priter++) {
			// find the maximum value
			int maxval = priter->max_val();
			// delete any solutions that aren't that value, and inc num_sol_eliminated
			std::list<struct solutionobj>::iterator solit = priter->solutions.begin();
			while (solit != priter->solutions.end()) {
				if (solit->answer != maxval) {
					solit = priter->solutions.erase(solit); // delete and advance
					checkallmaxelim++;
				} else { solit++; } // just advance
			}
			// if this PR has only one solution remaining, and it is unique, that solution can be applied! unlikely but possible
			if (((priter->solutions.size()) == 1) && (priter->solutions.front().allocs_encompassed == 1)) {
				for (std::list<class cell *>::iterator celliter = priter->solutions.front().allocation.begin(); celliter != priter->solutions.front().allocation.end(); celliter++) {
					int r = mygame.set_flag(*celliter);
					*thingsdone += bool(r);
					if (r == -1) { return 1; } else if (r == 2) { assert(0); return -2; }
				}
			}
		}
	}

	if (myruninfo.SCREEN == 3) {
		// only print the appropriate message if something was actually eliminated
		if (checkminelim) myprintfn(2, "ENDSOLVER: eliminated a solution for being too SMALL\n");
		if (checkmaxelim) myprintfn(2, "ENDSOLVER: eliminated a solution for being too LARGE\n");
		if (checkallminelim) myprintfn(2, "ENDSOLVER: determined using all MINIMUMS reaches the correct result\n");
		if (checkallmaxelim) myprintfn(2, "ENDSOLVER: determined using all MAXIMUMS reaches the correct result\n");
		int num_sol_eliminated = checkminelim + checkmaxelim + checkallminelim + checkallmaxelim;
		if (num_sol_eliminated) myprintfn(2, "ENDSOLVER: eliminated %i / %i solutions, %f%%\n", num_sol_eliminated, num_sol_start, 100. * float(num_sol_eliminated) / float(num_sol_start));
	}
	return 0;
}







// laboriously determine the % risk of each unknown cell and choose the one with the lowest risk to reveal
// can completely solve the puzzle, too; if it does, it clears/flags everything it knows for certain
// doesn't return cells, instead clears/flags them internally
// modeflag: 0=guess, 1=multicell, 2=endsolver
// return: 1=win/-1=loss/0=continue/-2=unexpected loss (winning is unlikely but possible)
int smartguess(struct game_stats * gstats, int * thingsdone, int * modeflag) {
	static struct riskholder myriskholder(myruninfo.get_SIZEX(), myruninfo.get_SIZEY());
	std::list<class cell *> interior_list = mygame.unklist;

	struct chain master_chain = chain();
	// steps 1/2/3 are done inside this function
	int r = strat_multicell_logic_and_chain_builder(&master_chain, thingsdone);
	if (*thingsdone != 0) {
		*modeflag = 1;
		return r;
	}


	// step 4: now that master_chain is refined, remove pod contents (border unk) from interior_unk
	// will still have some dupes (link cells) but much less than if I did this earlier
	for (std::list<struct pod>::iterator podit = master_chain.podlist.begin(); podit != master_chain.podlist.end(); podit++) {
		for (int i = 0; i < podit->size(); i++) {
			interior_list.remove(podit->cell_list[i]); // remove from interior_list by value (find and remove)
		}
	}

	float interior_risk = 150.;
	std::vector<struct podwise_return> retholder;	// holds the podwise_return objects I got back from recursion
	std::vector<struct chain> listofchains;			// holds each separate chain once they're linked and stuff
	bool use_endsolver = (mygame.get_mines_remaining() <= SMARTGUESS_ENDSOLVER_THRESHOLD_def);

	// if there are no interior cells AND it is not near the endgame, then skip the recursion
	if (!interior_list.empty() || use_endsolver || (GUESSING_MODE_var == 2)) {

		// step 5: iterate again, building links to anything within 5x5(only set MY links)
		for (std::list<struct pod>::iterator podit = master_chain.podlist.begin(); podit != master_chain.podlist.end(); podit++) {
			std::vector<std::list<struct pod>::iterator> around = master_chain.get_5x5_around(podit, true); // INCLUDE THE CORNERS
			for (int i = 0; i < around.size(); i++) {
				// for each pod 'otherpod' with root within 5x5 found...
				std::vector<std::vector<class cell *>> n = extract_overlap(podit->cell_list, around[i]->cell_list);
				// ... only create links within podit!
				for (int j = 0; j < n[2].size(); j++) {
					podit->add_link(n[2][j], around[i]->root);
				}
			}
		}

		// step 6: identify chains and sort the pods into a VECTOR of chains... 
		int numchains = master_chain.identify_chains();
		// smartguess/normal: turn "cell_list" into a simple number, and clear the actual list so it uses less memory while recursing
		// other modes: retain the cell_list information
		listofchains = master_chain.sort_into_chains(numchains, !(use_endsolver || (GUESSING_MODE_var == 2)));

		// step 7: for each chain, recurse (depth, chain, mode are only arguments) and get back list of answer allocations
		// handle the multiple podwise_retun objects, just sum their averages
		if (myruninfo.SCREEN == 3) myprintfn(2, "DEBUG: in smart-guess, # primary chains = %i \n", numchains);
		float border_allocation = 0;
		retholder.resize(numchains, podwise_return());
		for (int s = 0; s < numchains; s++) {
			recursion_safety_valve = false; // reset the flag for each chain
			struct podwise_return asdf = podwise_recurse(0, 0, &(listofchains[s]), use_endsolver);
			border_allocation += asdf.avg();
			if (use_endsolver || (GUESSING_MODE_var == 2)) { retholder[s] = asdf; } // store the podwise_return obj for later analysis
			if (recursion_safety_valve) { myprintfn(2, "WARNING: in smart-guess, chain %i aborted recursion early, might return incomplete/misleading data!\n", s); }
			if ((myruninfo.SCREEN == 3) || recursion_safety_valve) { myprintfn(2, "DEBUG: in smart-guess, chain %i with %i pods found %i answers\n", s, listofchains[s].podlist.size(), asdf.size()); }
			if (myruninfo.SCREEN == 3) { myprintfn(2, "DEBUG: in smart-guess, chain %i ran with %.3f%% efficiency\n", s, (100. * asdf.efficiency())); }
			gstats->smartguess_valves_tripped += recursion_safety_valve;
		}


		// step 8: if near the endgame, run ENDSOLVER
		// problem: each chain is solved independently, without regard for the total # of mines remaining to be placed or mines in other chains
		// solution: checks if any solutions fit perfectly, if in perfectmode it also finds & eliminates any invalid solutions
		// currently runs just if there are not many mines remaining, consider running if there are not many interior-list cells remaining??
		if (use_endsolver) {
			int p = strat_endsolver_and_solution_reducer_logic(&retholder, &interior_list, thingsdone);
			if (*thingsdone) {
				if (myruninfo.SCREEN == 3) myprintfn(2, "ENDSOLVER: did something!!\n");
				*modeflag = 2;
				return p;
			}
		}
		


		// step 9: calculate interior_risk, do statistics things
		// calculate the risk of a mine in any 'interior' cell (they're all the same)
		if (interior_list.empty()) {
			interior_risk = 150.;
		} else {

			////////////////////////////////////////////////////////////////////////
			// DEBUG/STATISTICS STUFF
			// question: how accurate is the count of the border mines? are the limits making the algorithm get a wrong answer?
			// answer: compare 'border_allocation' with a total omniscient count of the mines in the border cells
			// note: easier to find border mines by (bordermines) = (totalmines) - (interiormines)
			int interiormines = 0;
			// TODO: calculate this info, but instead of just averaging it, export it to a separate file
			// solver progress / estimated mines / actual mines
			// somehow incorporate chain length? how hard is it to count the mines in each chain individually?
			// TODO: eventually implement a "bias function" to correct for mis-estimating, probably depend on whole game progress, chain size, estimated mines
			for (std::list<class cell *>::iterator cellit = interior_list.begin(); cellit != interior_list.end(); cellit++) {
				if ((*cellit)->value == MINE)
				interiormines++;
			}
			int bordermines = mygame.get_mines_remaining() - interiormines;

			if (myruninfo.SCREEN == 3) myprintfn(2, "DEBUG: in smart-guess, border_avg/ceiling/border_actual = %.3f / %i / %i\n", border_allocation, mygame.get_mines_remaining(), bordermines);
			gstats->smartguess_attempts++;
			gstats->smartguess_diff += (border_allocation - float(bordermines)); // accumulating a less-negative number
			////////////////////////////////////////////////////////////////////////


			border_allocation = min(border_allocation, mygame.get_mines_remaining()); // border_alloc must be <= mines_remaining
			interior_risk = (float(mygame.get_mines_remaining()) - border_allocation) / float(interior_list.size()) * 100.;
		}

	} // end of "finding likely border mine allocation to determine interior risk" section


	// step 10: calculate risk for the border cells and identify cells tied for minimum risk
	std::pair<float, std::list<class cell *>> myriskreturn;
	std::list<class cell *> clearmelist;
	std::list<class cell *> flagmelist;
	if(GUESSING_MODE_var != 2) {
		// Option A: the risk for each individual cell is the avg/max risk from any of the pods it belongs to
		// smartguess/normal and smartguess/endsolver
		// iterate over "master chain", storing risk information into 'riskholder' (only read cell_list since it also holds the links)
		for (std::list<struct pod>::iterator podit = master_chain.podlist.begin(); podit != master_chain.podlist.end(); podit++) {
			float podrisk = podit->risk();
			for (int i = 0; i < podit->cell_list.size(); i++) {
				myriskholder.addrisk(podit->cell_list[i], podrisk);
			}
		}
	} else {
		// for each PR in retholder,
		for (int a = 0; a < retholder.size(); a++) {
			if (use_endsolver) {
				// perfectmode/endsolver
				// create the aggregate info structure in the PR from the exhaustive list of solutions

				// first, populate the list with 'empty' entries so I can later distinguish 'no data' from 'always clear'
				// for each pod in the corresponding chain,
				for (std::list<struct pod>::iterator podit = listofchains[a].podlist.begin(); podit != listofchains[a].podlist.end(); podit++) {
					// for each cell in that pod,
					for (int b = 0; b < podit->cell_list.size(); b++) {
						retholder[a].add_aggregate(podit->cell_list[b], 0);
					}
				}

				// second, actually create aggregate info from the solutions!
				retholder[a].agg_allocs = 1;
				// for each solution in the PR,
				for (std::list<struct solutionobj>::iterator solit = retholder[a].solutions.begin(); solit != retholder[a].solutions.end(); solit++) {
					// for each cell in that solution's allocation,
					for (std::list<class cell *>::iterator cellit = solit->allocation.begin(); cellit != solit->allocation.end(); cellit++) {
						retholder[a].add_aggregate(*cellit, 1);
					}
				}
				// i know that each solution has exactly 1 alloc so this is safe
				retholder[a].agg_allocs = retholder[a].solutions.size();
			}
			// perfectmode/endsolver and perfectmode/normal
			// calculate actual risk percentage for each cell from the aggregate info structure
			// also identify if any cells are flagged in EVERY solution, or in NO solutions
			// each PR object is guaranteed to not overlap with any others
			assert(retholder[a].total_alloc() == retholder[a].agg_allocs);
			for (std::list<struct aggregate_cell>::iterator aggit = retholder[a].agg_info.begin(); aggit != retholder[a].agg_info.end(); aggit++) {
				float cellrisk = 100. * float(aggit->times_flagged) / float(retholder[a].agg_allocs);
				assert(cellrisk <= 100.);
				assert(cellrisk >= 0.);
				if (cellrisk == 100.) {
					flagmelist.push_back(aggit->me);
				} else if (cellrisk == 0.) {
					clearmelist.push_back(aggit->me);
				} else {
					myriskholder.addrisk(aggit->me, cellrisk);
				}
			}
		}
	}
	// find the lowest risk of anything i've entered into the riskholder, as well as the cells that correspond to it
	// also clears/resets the myriskholder object for next time
	// need to do this unconditionally so the struct is clear for use next time!!
	myriskreturn = myriskholder.findminrisk();
	if (flagmelist.size() || clearmelist.size()) {
		// flag the flagme and clear the clearme and RETURN
		*modeflag = 2;
		if (myruninfo.SCREEN == 3) myprintfn(2, "DEBUG: from aggregate data, found %i clear and %i flag (but counts as endsolver)\n", clearmelist.size(), flagmelist.size());
		for (std::list<class cell *>::iterator cit = clearmelist.begin(); cit != clearmelist.end(); cit++) { // clear-list
			*thingsdone += 1;
			if (mygame.reveal(*cit) == -1) {
				myprintfn(2, "ERR: Unexpected loss during smartguess chain-solve!!\n"); assert(0); return -2;
			}
		}
		for (std::list<class cell *>::iterator fit = flagmelist.begin(); fit != flagmelist.end(); fit++) { // flag-list
			int r = mygame.set_flag(*fit);
			*thingsdone += bool(r);
			if (r == -1) { return 1; } else if (r == 2) { assert(0); return -2; }
		}
		return 0;
	}




	// step 11: decide between border and interior, call 'rand_from_list' and return
	if (myruninfo.SCREEN == 3) myprintfn(2, "DEBUG: in smart-guess, interior_risk = %.3f%%, border_risk = %.3f%%\n", interior_risk, myriskreturn.first);
	//if (interior_risk == 100.) {
	//	// unlikely but plausible (note: if interior is empty, interior_risk will be set at 150)
	//	// this is eclipsed by the 'maxguess' section, unless this scenario happens before the end-game
	//	results.flagme.insert(results.flagme.end(), interior_list.begin(), interior_list.end());
	//	results.method = 2;
	//} else 
	if (interior_risk < myriskreturn.first) {
		// interior is safer
		gstats->luck_value_mult *= (1. - (interior_risk / 100.)); // turn it from 'chance its a mine' to 'chance its safe'
		gstats->luck_value_sum += (1. - (interior_risk / 100.)); // turn it from 'chance its a mine' to 'chance its safe'
		// in smartguess, i don't care about how many cells are uncovered from one guess, just if it is loss or continue
		int r = mygame.reveal(rand_from_list(&interior_list));
		return ((r==-1) ? -1 : 0); // if -1, return -1; otherwise, return 0
	} else {
		// border is safer, or they are tied
		gstats->luck_value_mult *= (1. - (myriskreturn.first / 100.)); // turn it from 'chance its a mine' to 'chance its safe'
		gstats->luck_value_sum += (1. - (myriskreturn.first / 100.)); // turn it from 'chance its a mine' to 'chance its safe'
		int r = mygame.reveal(rand_from_list(&myriskreturn.second));
		return ((r == -1) ? -1 : 0); // if -1, return -1; otherwise, return 0
	}
	return 0; // this should be impossible to hit
}



// idea for 'aggregate data'
// for each cell, track "times it was flagged" vs "total times used"
// for flag, call add(1,1)
// for clear, call add(0,1)
// for disjoint/nonlink cells, if there are 2 mines in 4 cells, do the following
//		temp *= choose 2 from 4, or whatever
//		smartguess_return *= temp (mult into agg_allocs and also any agg_info entries that already exist)
//		add( cell, agg_allocs * mines/size , agg_allocs )
// in the end, EVERY cell should have the same sum of flags+clears, both link-cells and non-link-cells, top-level and down deep



//REMEMBER the venn diagram of how it fits together!!
//	perfectmode/normal encompasses smartguess/normal
//	perfectmode/endsolver encompases smartguess/endsolver encompasses smartguess/normal


// TODO: maybe after 'outof' member is removed, function will be add(cell, thislvl.agg_allocs/0/agg_allocs*ratio). yeah!


// recursively operates on a interconnected 'chain' of pods, and returns the list of all allocations it can find.
// 'recursion_safety_valve' is set whenever it would return something resulting from 10k or more solutions, then from that point
//		till the chain is done, it checks max 2 scenarios per level.
// smartguess/normal: returns list of solutionvalues w/ weights
// smartguess/endsolver: returns reduced/compressed/simplified roadmaps for the solutions + list of solutionvalues w/ weights
// perfectmode/normal: returns aggregate data for each cell in the chain + list of solutionvalues w/ weights
// perfectmode/endsolver: returns exhaustive list of all roadmaps, WAY more memory usage, dont even worry about optimizing it
//		^ don't store/calculate aggregate data, synthesize the data after step 8 because step 8 will eliminate some of the solutions
struct podwise_return podwise_recurse(int rescan_counter, int mines_from_above, struct chain * mychain, bool use_endsolver) {
	bool use_smartguess = (GUESSING_MODE_var == 2);
	// step 0: are we done?
	if (mines_from_above > mygame.get_mines_remaining()) {
		// means that the scenario above this is invalid; return a completely empty PR to signal this
		return podwise_return();
	} else if (mychain->podlist.empty()) {
		// this should be the "normal" end of the recursive branch, set 'effort' to 1
		struct podwise_return r = podwise_return(mines_from_above, 1);
		if (use_smartguess && !use_endsolver) { r.agg_allocs = 1; }
		return r;
	} else if (mychain->podlist.size() == 1) {
		// AFAIK this only happens if the initial top-level chain is a single pod. a pod that becomes disjoint in iteration should be handled in 3c.
		// this is guaranteed to have multiple allocations(size > mines), or else it would have been solved in singlecell logic 
		int m = mychain->podlist.front().mines;
		int s = mychain->podlist.front().size();
		int c = comb_int(m, s);
		if (!use_smartguess) {
			// smartguess/normal only cares about the answer; smartguess/endsolver wants the list of cells but can't use it because it is multiple allocs
			return podwise_return(m, c);
		} else if (use_smartguess && !use_endsolver) {
			// perfecmode/normal: return the aggregate data
			struct podwise_return r(m, c);
			r.agg_allocs = c;
			// for each cell in the pod, add its info
			for (int i = 0; i < s; i++) {
				r.add_aggregate(mychain->podlist.front().cell_list[i], float(m) / float(s));
			}
			return r;
		} else if (use_smartguess && use_endsolver) {
			// perfectmode, endsolver: return the exhaustive list of all solutions that satisfy this, each has allocs=1
			struct podwise_return r;
			r.solutions.resize(c, solutionobj(m, 1)); // now has correct # branches, just needs the actual cells
			r.effort = 1;
			std::list<std::vector<int>> ret = comb(m, s);
			// for each solution,
			for (std::list<struct solutionobj>::iterator solit = r.solutions.begin(); solit != r.solutions.end(); solit++) {
				// for each int in ret.front(),
				for (int f = 0; f < ret.front().size(); f++) {
					// use that int as index in cell_list and push to the end of the solution in question
					solit->allocation.push_back(mychain->podlist.front().cell_list[ret.front()[f]]);
				}
				ret.pop_front();
			}
			return r;
		}
	}

	/*
	// step 1: is it time to rescan the chain?
	if (rescan_counter < CHAIN_RECHECK_DEPTH) {
		rescan_counter++;
	} else {
		rescan_counter = 0;
		int r = mychain->identify_chains();
		if (r > 1) {
			// if it has broken into 2 or more distinct chains, seperate them to operate recursively on each! much faster than the whole
			std::vector<struct chain> chain_list = mychain->sort_into_chains(r, false); // where i'll be sorting them into

			// handle the multiple podwise_return objects, just sum their averages and total lengths
			// NOTE: this same structure/method used at highest-level, when initially invoking the recursion
			float sum = 0;
			int effort_sum = 0;
			int alloc_product = 1;
			for (int s = 0; s < r; s++) {
				struct podwise_return asdf = podwise_recurse(rescan_counter, &(chain_list[s]), use_smartguess, use_endsolver);
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
	*/

	std::list<struct pod>::iterator temp = mychain->podlist.begin();
	// NOTE: if there are no link cells, then front() is disjoint!
	// AFAIK, this should never happen... top-level call on disjoint will be handled step 0, any disjoint created will be handled step 3c
	assert(!(temp->links.empty()));
	//if (temp->links.empty()) {
	//	myprintfn(2, "ERR: in smartguess recursion, this should have never happened\n");
	//	assert(0);
	//	int f = temp->mines;
	//	mychain->podlist.erase(temp);
	//	struct podwise_return asdf = podwise_recurse(rescan_counter, mychain);
	//	asdf += f;
	//	return asdf;
	//}

	// step 2: pick a pod, find all ways to saturate it
	// NOTE: first-level optimization is that only link/overlap cells are considered... specific allocations within non-link cells are
	//       ignored, since they have no effect on placing flags in any other pods.
	// NOTE: second-level optimization is eliminating redundant combinations of 'equivalent links', i.e. links that span the same pods
	//       are interchangeable, and I don't need to test all combinations of them.
	// list of lists of list-iterators which point at links in the link-list of front() (that's a mouthfull)

	// perfectmode needs to NOT use T2 scenario optimization, simply so I can get the data out. 
	//TODO:	is there a way to detect the equiv links outside the scenario generator so i can know this while still reducing the recursive branching?
	std::list<struct scenario> scenarios = temp->find_scenarios(!use_smartguess);

	// where results from each scenario recursion are accumulated
	struct podwise_return retval = podwise_return();
	int whichscenarioSUCCESSFUL = 1; // for the purpose of 
	int whichscenarioREALLY = 1; // int to count how many scenarios i've tried
	for (std::list<struct scenario>::iterator scit = scenarios.begin(); scit != scenarios.end(); scit++) {
		// for each scenario found above, make a copy of mychain, then...
		struct podwise_return thislvl(0,1); // create PR with one entry, holds everything found thislvl
		thislvl.effort = 0; // this does not represent a 'leaf' so it must be 0

		thislvl.solutions.front().answer = 0;	// holds the # of flags placed this lvl; inc with thislvl += 1, works even if branched
		// represents how many allocations are possible that reach this specific answer thislvl
		thislvl.solutions.front().allocs_encompassed = scit->allocs_encompassed;
		// if perfectmode/normal, using aggregate data, and must initialize this value
		if (use_smartguess && !use_endsolver) { thislvl.agg_allocs = scit->allocs_encompassed; } 
		// dont need to declare or init this, it just exists already; holds all cells flagged as part of this solution
		//thislvl.solutions.front().allocation; 

		struct chain copychain = *mychain; // a copy of the chain that i can modify, and then pass to deeper recursion
		// lower needs to be declared here because 'goto' and labels are weird :/
		struct podwise_return lower; // will receive PR object from deeper recursion

		// list of links that need to be removed from the chain; first is link, second is (true=flag, false=reveal)
		std::list<std::pair<struct link, bool>> links_to_flag_or_clear;

		std::list<struct pod>::iterator frontpod = copychain.podlist.begin(); // self-explanatory

		// step 3a: apply the changes according to the scenario by moving links from front() pod to links_to_flag_or_clear
		for (std::list<std::list<struct link>::iterator>::iterator r = scit->scenario_links_to_flag.begin(); r != scit->scenario_links_to_flag.end(); r++) {
			// turn iterator-over-list-of-iterators into just an iterator
			std::list<struct link>::iterator linkit = *r;
			// copy it from front().links to links_to_flag_or_clear as 'flag'...
			links_to_flag_or_clear.push_back(std::pair<struct link, bool>(*linkit, true));
			// ...and delete the original from the list of links as well as the cell_list
			frontpod->remove_link(linkit->link_cell, true);
		}
		// then copy all remaining links to links_to_flag_or_clear as 'clear'
		for (std::list<link>::iterator linkit = frontpod->links.begin(); linkit != frontpod->links.end(); linkit++) {
			links_to_flag_or_clear.push_back(std::pair<struct link, bool>(*linkit, false));
			if (use_smartguess || use_endsolver) {
				// for perfectmode, i need to delete all links from the cell_list so only non-links remain
				int s = frontpod->cell_list.size();
				for (int i = 0; i < s; i++) {
					if (frontpod->cell_list[i] == linkit->link_cell) {
						// when found, delete it by copying the end onto it
						if (i != (s - 1)) { frontpod->cell_list[i] = frontpod->cell_list.back(); }
						frontpod->cell_list.pop_back();
						break;
					}
				}
			}
		}


		// NOTE: all of these blocks depend on mines/cell_list/size being correct after moving links from the pod to the queue
		// frontpod erases itself and increments by how many mines it has that aren't in the scenario
		// because it is being saturated, by definition it becomes disjoint
		thislvl += frontpod->mines; // add flags for the non-link cells to all solutions this level

		// smartguess/normal: do nothing extra
		if (!use_smartguess && use_endsolver) {
			// smartguess/endsolver: will only have 1 solution, only add if allocs_encompassed = 1 (includes req that mines=size), only add if mines=size>0
			if ((thislvl.solutions.front().allocs_encompassed == 1) && (frontpod->mines > 0)) {
				thislvl += frontpod->cell_list;
			}
		} else if (use_smartguess && !use_endsolver) { 
			// if perfectmode/normal,
			// dont need to calculate c or multiply thru thislvl, already set above...
			// for each cell remaining in the pod, add its info
			for (int i = 0; i < frontpod->size(); i++) {
				thislvl.add_aggregate(frontpod->cell_list[i], float(frontpod->mines) / float(frontpod->size()));
			}
		} else if (use_smartguess && use_endsolver) {
			// if perfectmode/endsolver, 
			// set allocs_encompassed to 1, iterate over the non-link cells and branch into several solutions inside thislvl
			// need to branch here, but don't need to convolute... guaranteed that thislvl has exactly 1 solution going into this block
			int m = frontpod->mines; int s = frontpod->size(); int c = scit->allocs_encompassed;

			thislvl.solutions.front().allocs_encompassed = 1; // will be copied 
			thislvl.solutions.resize(c, thislvl.solutions.front()); // now its the right size, just needs the actual cells
			std::list<std::vector<int>> ret = comb(m, s);
			// for each solution,
			for (std::list<struct solutionobj>::iterator solit = thislvl.solutions.begin(); solit != thislvl.solutions.end(); solit++) {
				// for each int in ret.front(),
				for (int f = 0; f < ret.front().size(); f++) {
					// use that int as index in cell_list and push to the end of the solution in question
					solit->allocation.push_back(frontpod->cell_list[ret.front()[f]]);
				}
				ret.pop_front();
			}
		}

		copychain.podlist.erase(frontpod);


		// step 3b: iterate along links_to_flag_or_clear, removing the links from any pods they appear in...
		// there is high possibility of duplicate links being added to the list, but its handled just fine
		// decided to run depth-first (stack) instead of width-first (queue) because of the "looping problem"
		// NOTE: this is probably the most complex and delicate part of the whole MinesweeperSolver, the "looping problem" was 
		//		a real bitch to solve and I can't quite remember WHY arranging it like this works. So, don't change anything
		//		that adds or removes from links_to_flag_or_clear!
		while (!links_to_flag_or_clear.empty()) {
			// for each link 'blink' to be handled...
			std::pair<struct link, bool> blink = links_to_flag_or_clear.front();
			links_to_flag_or_clear.pop_front();

			class cell * sharedcell = blink.first.link_cell;
			bool thislinkwassuccesfullyflaggedandremovedfromapod = false;
			bool thislinkwassuccesfullyclearedandremovedfromapod = false;
			// ...go to every pod with a root 'rootit' referenced in 'blink' and remove the link from them!
			for (std::list<class cell *>::iterator rootit = blink.first.linked_roots.begin(); rootit != blink.first.linked_roots.end(); rootit++) {
				std::list<struct pod>::iterator activepod = copychain.root_to_pod(*rootit);
				if (activepod == copychain.podlist.end()) { continue; }
				if (activepod->remove_link(sharedcell, blink.second)) { continue; } // if link wasn't found, just move on... will happen ALOT
				thislinkwassuccesfullyflaggedandremovedfromapod = blink.second;
				thislinkwassuccesfullyclearedandremovedfromapod = !blink.second; // I think this will work? not certain

				// step 3c: after each change, see if activepod has become 100/0/disjoint/negative... if so, modify podlist and links_to_flag_or_clear
				//		when finding disjoint, delete it and inc "flags this scenario" accordingly
				//		when finding 100/0, store the link-cells, delete it, and inc "flags this scenario" accordingly
				//		when finding pods with NEGATIVE risk, means that this scenario is invalid. simply GOTO after step 4, begin next scenario.

				// after a link is removed from a pod, scan for 'special state' that needs handling
				// add each of its links to the STACK if something happened that needs handling, and delete the pod in question
				bool foobar = true;
				int r = activepod->scan();
				switch (r) {
				case 1: // risk is negative; initial scenario was invalid... just abort this scenario
					goto LABEL_END_OF_THE_SCENARIO_LOOP;
				case 2: // risk = 0
					foobar = false;
					//for (std::list<link>::iterator linkit = activepod->links.begin(); linkit != activepod->links.end(); linkit++) {
					//	links_to_flag_or_clear.push_front(std::pair<struct link, bool>(*linkit, false));
					//}
					//// no inc because I know there are no mines here
					//copychain.podlist.erase(activepod);
					//break;
				case 3: // risk = 100
						// add the remaining links in (*activepod) to links_to_flag_or_clear
						// if risk = 0, will add with 'false'... if risk = 100, will add with 'true'
					for (std::list<link>::iterator linkit = activepod->links.begin(); linkit != activepod->links.end(); linkit++) {
						std::pair<struct link, bool> t(*linkit, foobar);
						// need to add myself to the link so i will be looked at later and determined to be disjoint
						// hopefully fixes the looping-problem! IT DOES!
						t.first.linked_roots.push_back(activepod->root);
						
						// BEFORE ADDING, check: are there any contradictory instructions in the pipeline???
						for (std::list<std::pair<struct link, bool>>::iterator queit = links_to_flag_or_clear.begin(); queit != links_to_flag_or_clear.end(); queit++) {
							if ((queit->first.link_cell == t.first.link_cell) && (queit->second != t.second)) {
								// abort with extreme rapidity
								goto LABEL_END_OF_THE_SCENARIO_LOOP;
							}
						}

						links_to_flag_or_clear.push_front(t);
					}
					// no inc because flagsthislvl is inced ONCE if a link is removed from any pods
					break;
				case 4: // DISJOINT, all its links have been consumed, only non-link cells remain
					thislvl += activepod->mines; // add flags to all solutions this level
					int c = comb_int(activepod->mines, activepod->size());

					// need to do wildly different things depending on mode of operation:
					if (!use_smartguess && !use_endsolver) {
						thislvl *= c; // multiply in the multiple allocations
					} else if (!use_smartguess && use_endsolver) {
						// smartguess/endsolver: will only have 1 solution, only add if allocs_encompassed = 1 (includes req that mines=size), only add if mines=size>0
						thislvl *= c; // multiply in the multiple allocations
						if ((thislvl.solutions.front().allocs_encompassed == 1) && (activepod->mines > 0)) {
							thislvl += activepod->cell_list;
						}
					} else if (use_smartguess && !use_endsolver) {
						// perfectmode/normal: multiply into agg_allocs and add aggregate info
						thislvl *= c; // multiply in the multiple allocations
						// for each cell remaining in the pod, add its info
						for (int i = 0; i < activepod->size(); i++) {
							thislvl.add_aggregate(activepod->cell_list[i], float(activepod->mines) / float(activepod->size()));
						}
					} else if (use_smartguess && use_endsolver) {
						// perfectmode/endsolver: assume multiple branches exist, create more here, don't mult allocs_encompassed thru (force it to be 1)
						// branching is done by copying thislvl, adding different cells to each copy, then recombining them

						std::list<struct podwise_return> copyholder(c, thislvl); // need to make 'c' copies

						std::list<std::vector<int>> ret = comb(activepod->mines, activepod->size()); // identify the many ways to put mines in the non-link cells
						// each podwise_return object uses one entry from 'ret', the front() one
						for (std::list<struct podwise_return>::iterator prit = copyholder.begin(); prit != copyholder.end(); prit++) {
							// for each int in ret.front(),
							for (int f = 0; f < ret.front().size(); f++) {
								// use that int as index in cell_list and push to the end of the solution in question
								*prit += activepod->cell_list[ret.front()[f]];
							}
							ret.pop_front();
							// now that this copy has been modified, append it onto the first one!
							if (prit != copyholder.begin()) {
								copyholder.front().solutions.splice(copyholder.front().solutions.begin(), prit->solutions);
							}
						}
						// FINALLY, put the new set of solutions (after creating the many branches) back into the proper place
						thislvl.solutions = copyholder.front().solutions;
					}

					copychain.podlist.erase(activepod);
					break; // not needed, but whatever
				}
			} // end of iteration on shared_roots

			// step 3d: now, deal with the actual link cell:
			if (thislinkwassuccesfullyflaggedandremovedfromapod) {
				thislvl += 1; // add flags to all solutions this level
				// smartguess/normal, do nothing extra
				if (!use_smartguess && use_endsolver) {
					// smartguess/endsolver, only 1 solution exists, add to it only if there is only 1 allocation
					if (thislvl.solutions.front().allocs_encompassed == 1) {
						thislvl += sharedcell;
					}
				} else if (use_smartguess && !use_endsolver) {
					// perfectmode/normal, add to aggregate data as flagged
					thislvl.add_aggregate(sharedcell, 1);
				} else if (use_smartguess && use_endsolver) {
					// perfectmode/endsolver, assume multiple branches exist, add this onto all
					thislvl += sharedcell;
				}
			} else if (thislinkwassuccesfullyclearedandremovedfromapod) {
				// add to aggregate data as cleared
				if (use_smartguess && !use_endsolver) {
					thislvl.add_aggregate(sharedcell, 0);
				}
			}
		}// end of iteration on links_to_flag_or_clear


		// step 4: recurse! when it returns, combine the 'lower' info with 'thislvl' info (VERTICAL combining)
		// with first 3 modes, there is only one solutions entry thislvl; with perfectmode/endsolver, all solutions have the same answer value
		lower = podwise_recurse(rescan_counter, (thislvl.solutions.front().answer + mines_from_above), &copychain, use_endsolver);
		
		// NOTE: if lower.solutions.empty(), that means that thislvl (this scenario) is invalid!!
		// either this scenario directly places more mines than there are remaining in the game, or all branches that descend from this one
		// place too many mines. Or, one of the scenario allocations force some other pod into an invalid situation.
		if (lower.solutions.empty()) { goto LABEL_END_OF_THE_SCENARIO_LOOP; }

		//need to clean up & make transparent how "this level" is combined(VERTICALLY) with result from levels below 
		//	'allocsthislvl' multiplied into allocs_encompassed for each solution returned from below (except perfectmode/endsolver)
		//	mult my agg_allocs into it, mult its agg_allocs into me, then merge lists (?) (there will not be anything to collapse here)
		//	'cellsthislvl' appended to allocation for each solution returned from below (except perfectmode/endsolver)

		if (use_smartguess && use_endsolver) {
			// must convolute: multiply lower w/ multiple solutions * thislvl w/ multiple solutions
			// but all thislvl have the same answer# i'm pretty sure, and all have only 1 alloc
			// resulting answer is in lower

			// make s copies of lower, then each copy gets one solution from thislvl, then roll them all into one
			std::list<struct podwise_return> copyholder(thislvl.solutions.size(), lower); // need to make 's' copies
			// each podwise_return object uses one entry from 'thislvl.solutions'
			std::list<struct solutionobj>::iterator solit = thislvl.solutions.begin();
			for (std::list<struct podwise_return>::iterator prit = copyholder.begin(); prit != copyholder.end(); prit++) {
				//*prit += solit->answer; // add the number of flags placed to each solution in the copy
				*prit += solit->allocation; // add the actual list of cells to each solution in the copy
				solit++; // move to the next solution from thislvl
				// now that this copy has been modified, append it onto the first one!
				if (prit != copyholder.begin()) {
					copyholder.front().solutions.splice(copyholder.front().solutions.begin(), prit->solutions);
				}
			}
			// FINALLY, put the new set of solutions (after creating the many branches) back into the proper place
			lower.solutions = copyholder.front().solutions;
		} else {
			// all 3 modes use this
			//lower += int(thislvl.solutions.front().answer); // inc the answer for each by how many flags placed this level
			if (use_smartguess && !use_endsolver) {
				//perfectmode/normal, lower *= allocs, thislvl *= lower(before mult), merge aggregate data
				int t = lower.agg_allocs;
				lower *= thislvl.agg_allocs;
				thislvl *= t; // cross-multiply into eachother
				lower.agg_info.merge(thislvl.agg_info, sort_aggregate_cell); // merge
			} else {
				//smartguess/normal, lower *= allocs //smartguess/endsolver, lower *= allocs
				lower *= thislvl.solutions.front().allocs_encompassed; // multiply the # allocations for this scenario into each
			}
			if (!use_smartguess && use_endsolver && (thislvl.solutions.front().allocs_encompassed == 1)) {
				lower += thislvl.solutions.front().allocation; // add these cells into the answer for each
			}
		}



		// step 5: combine the 'lower' info into 'retval', (HORIZONTAL combining)
		//need to clean up & make transparent how "this level + below" are combined(HORIZONTALLY) with eachother to make retval 
		//	solutions list is appended
		//	effort is combined
		//  agg_allocs is combined
		//	NO MULTIPLY, just merge lists (note: collapsing is done all at once, just before returning)

		// assert that each scenario's result has the same length of agg_info
		assert(retval.agg_info.size() == ((whichscenarioSUCCESSFUL -1) * lower.agg_info.size()));
			
		retval.effort += lower.effort;
		retval.agg_allocs += lower.agg_allocs;
		retval.agg_info.merge(lower.agg_info, sort_aggregate_cell); // merge
		retval.solutions.splice(retval.solutions.end(), lower.solutions); // append list of solutions
		

		// if the safety valve has been activated, only try RECURSION_SAFE_WIDTH valid scenarios each level at most
		if (recursion_safety_valve && (whichscenarioSUCCESSFUL >= RECURSION_SAFE_WIDTH)) { break; }
		whichscenarioSUCCESSFUL++; // don't want this to inc on an invalid scenario
	LABEL_END_OF_THE_SCENARIO_LOOP:
		whichscenarioREALLY++; // need to have something here or else the label/goto gets all whiney
	}// end of iteration on the various ways to saturate frontpod


	// step 6: return the answer found by recursing on every level below this one
	// before returning, need to collapse/combine aggregate info entries that represent the same cell
	// retval isn't actually valid until after this
	if (use_smartguess && !use_endsolver) {
		if (retval.agg_info.size() >= 2) {
			std::list<struct aggregate_cell>::iterator a = retval.agg_info.begin();
			std::list<struct aggregate_cell>::iterator b = a; a++; // b points to 0, a points to 1
			while (a != retval.agg_info.end()) {
				if (equivalent_aggregate_cell(*a, *b)) {
					// combine a into b
					b->times_flagged += a->times_flagged;
					a = retval.agg_info.erase(a); // both delete a and advance
				} else {
					a++; b++;
				}
			}
		}
	}

	// if I have spent too much effort calculating all the possibilities, then trip the safety valve
	if (retval.effort > RECURSION_SAFETY_LIMITER) { recursion_safety_valve = true; }
	return retval;
}



// **************************************************************************************
// solver rules & strategies


// STRATEGY: single-cell logic
// firstly, if an X-effective cell is next to X unknowns, flag them all.
// secondly, if an X-adjacency cell is next to X flags (AKA effective = 0), all remaining unknowns are SAFE and can be revealed.
// veryveryvery simple to understand. internally identifies and flags/clears cells. also responsible for setting state to "satisfied"
// when appropriate. no special stats to track here, except for the "singlecell total action count".
// return: 1=win/-1=loss/0=continue (cannot lose tho, unless something is seriously out of whack)
int strat_singlecell(class cell * me, int * thingsdone) {
	std::vector<class cell *> unk = mygame.filter_adjacent(me, UNKNOWN);
	int r = 0;
	// strategy 1: if an X-adjacency cell is next to X unknowns, flag them all
	if ((me->get_effective() != 0) && (me->get_effective() == unk.size())) {
		// flag all unknown cells
		for (int i = 0; i < unk.size(); i++) {
			r = mygame.set_flag(unk[i]);
			*thingsdone += bool(r); // inc by # of cells flagged (one)
			if (r == -1) { return 1; } // validated win! time to return!
		}
		unk.clear(); // must clear the unklist for next stage to work right
	}

	// strategy 2: if an X-adjacency cell is next to X flags, all remaining unknowns are NOT flags and can be revealed
	if (me->get_effective() == 0) {
		// reveal all adjacent unknown squares
		for (int i = 0; i < unk.size(); i++) {
			r = mygame.reveal(unk[i]);
			if (r == -1) {
				myprintfn(2, "ERR: Unexpected loss during SC satisfied-reveal, must investigate!!\n");
				assert(1 == 2);
				return -1;
			}
			*thingsdone += r; // inc by # of cells revealed
		}
		me->set_status_satisfied();
	}
	return 0;
}



// STRATEGY: 121 cross
// looks for a specific arrangement of visible/unknown cells; if found, I can clear up to 2 cells.
// unlike the other strategies, this isn't based in logic so much... this is just a pattern I noticed.
// return: 1=win/-1=loss/0=continue (except cannot win, ever, and cannot lose unless something is seriously out of whack)
// NEW FORMAT: IN-PLACE VERSION, clears the cells here
int strat_121_cross(class cell * center, struct game_stats * gstats, int * thingsdone) {
	if (center->get_effective() != 2) { return 0; }
	std::vector<class cell *> adj = mygame.get_adjacent(center);
	if (adj.size() == 3) // must be in a corner
		return 0;

	int r = 0; int s = 0;
	class cell * right =mygame.cellptr((center->x) + 1, center->y);
	class cell * left =	mygame.cellptr((center->x) - 1, center->y);
	class cell * down = mygame.cellptr(center->x, (center->y) + 1);
	class cell * up =	mygame.cellptr(center->x, (center->y) - 1);

	// assuming 121 in horizontal line:
	if ((right != NULL) && (left != NULL) && (right->get_status() == VISIBLE) && (left->get_status() == VISIBLE)
		&& (right->get_effective() == 1) && (left->get_effective() == 1)) {
		r = mygame.reveal(down);
		s = mygame.reveal(up);
		if ((r == -1) || (s == -1)) {
			myprintfn(2, "ERR: Unexpected loss during MC 121-cross, must investigate!!\n"); assert(0);
			return -1;
		} else {
			gstats->strat_121 += (r != 0 || s != 0);
			*thingsdone += (r + s);
			return 0;
		}
	}

	// assuming 121 in vertical line:
	if ((down != NULL) && (up != NULL) && (down->get_status() == VISIBLE) && (up->get_status() == VISIBLE)
		&& (down->get_effective() == 1) && (up->get_effective() == 1)) {
		r = mygame.reveal(right);
		s = mygame.reveal(left);
		if ((r == -1) || (s == -1)) {
			myprintfn(2, "ERR: Unexpected loss during MC 121-cross, must investigate!!\n"); assert(0);
			return -1;
		} else {
			gstats->strat_121 += (r != 0 || s != 0);
			*thingsdone += (r + s);
			return 0;
		}
	}
	return 0;
}



// STRATEGY: nonoverlap-flag
//If two cells are X and X + 1, and the unknowns around X + 1 fall inside unknowns around X except for ONE, that
//non-overlap cell must be a mine
//Expanded to the general case: if two cells are X and X+Z, and X+Z has exactly Z unique cells, then all those cells must be mines
// Compare against 5x5 region minus corners.
// X(other) = 1/2/3/4,  Z = 1/2/3/4/5/6
// return: 1=win/-1=loss/0=continue (except cannot lose here because it doesn't reveal cells here)
// NEW FORMAT: IN-PLACE VERSION, clears the cells here
int strat_nonoverlap_flag(class cell * center, struct game_stats * gstats, int * thingsdone) {
	if ((center->get_effective() < 2) || (center->get_effective() == 8)) { return 0; } // center must be 2-7
	std::vector<class cell *> me_unk = mygame.filter_adjacent(center, UNKNOWN);
	std::vector<class cell *> other_unk;
	for (int b = -2; b < 3; b++) { for (int a = -2; a < 3; a++) {
		if (((a == -2 || a == 2) && (b == -2 || b == 2)) || (a == 0 && b == 0)) { continue; } // skip myself and also the corners
		class cell * other = mygame.cellptr(center->x + a, center->y + b);
		if ((other == NULL) || (other->get_status() != VISIBLE)) { continue; }			// must exist and be already revealed
		if ((other->get_effective() == 0) || (other->get_effective() > 4)) { continue; }	// other must be 1/2/3/4
		int z = center->get_effective() - other->get_effective();
		if (z < 1) { continue; }														// z must be 1 or greater

		other_unk = mygame.filter_adjacent(other, UNKNOWN);

		std::vector<std::vector<class cell *>> nonoverlap = extract_overlap(me_unk, other_unk);
		// checking if OTHER is a subset of ME, AKA ME has some extra unique cells
		if (nonoverlap[0].size() == z) {
			gstats->strat_nov_flag++;
			for (int i = 0; i < z; i++) {
				int r = mygame.set_flag(nonoverlap[0][i]);
				*thingsdone += bool(r); // inc once for each flag placed
				if (r == -1) { return 1; }
			}
			return 0;
		}
	}}

	return 0;
}




// STRATEGY: nonoverlap-safe
//If the adj unknown tiles of a 1/2/3 -square are a pure subset of the adj unknown tiles of another
//square with the same value, then the non-overlap section can be safely revealed!
//Compare against any other same-value cell in the 5x5 region minus corners
// return: 1=win/-1=loss/0=continue (except cannot win, ever, and cannot lose unless something is seriously out of whack)
// NEW FORMAT: IN-PLACE VERSION, clears the cells here
int strat_nonoverlap_safe(class cell * center, struct game_stats * gstats, int * thingsdone) {
	if (center->get_effective() > 3) { return 0; } // only works for center = 1/2/3
	std::vector<class cell *> me_unk = mygame.filter_adjacent(center, UNKNOWN);
	std::vector<class cell *> other_unk;
	int retme = 0;
	for (int b = -2; b < 3; b++) { for (int a = -2; a < 3; a++) {
		if (((a == -2 || a == 2) && (b == -2 || b == 2)) || (a == 0 && b == 0)) { continue; } // skip myself and also the corners
		class cell * other = mygame.cellptr(center->x + a, center->y + b);
		if ((other == NULL) || (other->get_status() != VISIBLE)) { continue; }	// must exist and be already revealed
		if (center->get_effective() != other->get_effective()) { continue; }	// the two being compared must have same effective value
		other_unk = mygame.filter_adjacent(other, UNKNOWN);
		if (me_unk.size() >= other_unk.size()) { continue; } // shortcut, can't be subset if it's bigger or equal

		std::vector<std::vector<class cell *>> nonoverlap = extract_overlap(me_unk, other_unk);
		// checking if ME is a subset of OTHER
		if (nonoverlap[0].empty() && !(nonoverlap[1].empty())) {
			int retme_sub = 0;
			for (int i = 0; i < nonoverlap[1].size(); i++) {
				int r = mygame.reveal(nonoverlap[1][i]);
				if (r == -1) {
					myprintfn(2, "ERR: Unexpected loss during MC nonoverlap-safe, must investigate!!\n"); assert(0);
					return -1;
				}
				retme_sub += r;
			}
			*thingsdone += retme_sub; // increment by how many were cleared
			gstats->strat_nov_safe += bool(retme_sub);
			return 0;
			//me_unk = mygame.filter_adjacent(center, UNKNOWN); // update me_unk, then continue iterating thru the 5x5
		}
	}}

	return 0;
}



// I determined that the "queueing strategy" has no appreciable benefits compared to the "inline strategy"
/*
// return: 1=win/-1=loss/0=continue (except cannot win or lose here because it doesn't reveal cells here)
// NEW FORMAT: QUEUEING VERSION, returns the cells to be cleared all at once
int strat_121_cross_Q(class cell * center, struct game_stats * gstats, std::list<class cell *> * clearlist) {
	if (center->get_effective() != 2) { return 0; }
	std::vector<class cell *> adj = mygame.get_adjacent(center);
	if (adj.size() == 3) // must be in a corner
		return 0;

	int r = 0; int s = 0;
	class cell * right = mygame.cellptr((center->x) + 1, center->y);
	class cell * left = mygame.cellptr((center->x) - 1, center->y);
	class cell * down = mygame.cellptr(center->x, (center->y) + 1);
	class cell * up = mygame.cellptr(center->x, (center->y) - 1);

	// assuming 121 in horizontal line:
	if ((right != NULL) && (left != NULL) && (right->get_status() == VISIBLE) && (left->get_status() == VISIBLE)
		&& (right->get_effective() == 1) && (left->get_effective() == 1)) {
		bool t = false;
		if ((up != NULL) && (up->get_status() == UNKNOWN)) { t = true; clearlist->push_back(up); }
		if ((down != NULL) && (down->get_status() == UNKNOWN)) { t = true; clearlist->push_back(down); }
		if (t) { gstats->strat_121++; }
		return 0;
	}

	// assuming 121 in vertical line:
	if ((down != NULL) && (up != NULL) && (down->get_status() == VISIBLE) && (up->get_status() == VISIBLE)
		&& (down->get_effective() == 1) && (up->get_effective() == 1)) {
		bool t = false;
		if ((left != NULL) && (left->get_status() == UNKNOWN)) { t = true; clearlist->push_back(left); }
		if ((right != NULL) && (right->get_status() == UNKNOWN)) { t = true; clearlist->push_back(right); }
		if (t) { gstats->strat_121++; }
		return 0;
	}
	return 0;
}

// return: 1=win/-1=loss/0=continue (except cannot win or lose here because it doesn't reveal cells here)
// NEW FORMAT: QUEUEING VERSION, returns the cells to be cleared all at once
int strat_nonoverlap_flag_Q(class cell * center, struct game_stats * gstats, std::list<class cell *> * flaglist) {
	if ((center->get_effective() < 2) || (center->get_effective() == 8)) { return 0; } // center must be 2-7
	std::vector<class cell *> me_unk = mygame.filter_adjacent(center, UNKNOWN);
	std::vector<class cell *> other_unk;
	for (int b = -2; b < 3; b++) {
		for (int a = -2; a < 3; a++) {
			if (((a == -2 || a == 2) && (b == -2 || b == 2)) || (a == 0 && b == 0)) { continue; } // skip myself and also the corners
			class cell * other = mygame.cellptr(center->x + a, center->y + b);
			if ((other == NULL) || (other->get_status() != VISIBLE)) { continue; }			// must exist and be already revealed
			if ((other->get_effective() == 0) || (other->get_effective() > 4)) { continue; }	// other must be 1/2/3/4
			int z = center->get_effective() - other->get_effective();
			if (z < 1) { continue; }														// z must be 1 or greater

			other_unk = mygame.filter_adjacent(other, UNKNOWN);

			std::vector<std::vector<class cell *>> nonoverlap = extract_overlap(me_unk, other_unk);
			// checking if OTHER is a subset of ME, AKA ME has some extra unique cells
			if (nonoverlap[0].size() == z) {
				gstats->strat_nov_flag++;
				flaglist->insert(flaglist->end(), nonoverlap[0].begin(), nonoverlap[0].end());
				return 0;
			}
		}
	}

	return 0;
}

// return: 1=win/-1=loss/0=continue (except cannot win or lose here because it doesn't reveal cells here)
// NEW FORMAT: QUEUEING VERSION, returns the cells to be cleared all at once
int strat_nonoverlap_safe_Q(class cell * center, struct game_stats * gstats, std::list<class cell *> * clearlist) {
	if (center->get_effective() > 3) { return 0; }
	std::vector<class cell *> me_unk = mygame.filter_adjacent(center, UNKNOWN);
	std::vector<class cell *> other_unk;
	int retme = 0;
	for (int b = -2; b < 3; b++) { for (int a = -2; a < 3; a++) {
		if (((a == -2 || a == 2) && (b == -2 || b == 2)) || (a == 0 && b == 0)) { continue; } // skip myself and also the corners
		class cell * other = mygame.cellptr(center->x + a, center->y + b);
		if ((other == NULL) || (other->get_status() != VISIBLE)) { continue; }	// must exist and be already revealed
		if (center->get_effective() != other->get_effective()) { continue; }	// the two being compared must have same effective value
		other_unk = mygame.filter_adjacent(other, UNKNOWN);
		if (me_unk.size() >= other_unk.size()) { continue; } // shortcut, can't be subset if it's bigger or equal

		std::vector<std::vector<class cell *>> nonoverlap = extract_overlap(me_unk, other_unk);
		// checking if ME is a subset of OTHER
		if (nonoverlap[0].empty() && !(nonoverlap[1].empty())) {
			clearlist->insert(clearlist->end(), nonoverlap[1].begin(), nonoverlap[1].end());
			gstats->strat_nov_safe++;
			return 0;
			//me_unk = mygame.filter_adjacent(center, UNKNOWN); // update me_unk, then continue iterating thru the 5x5
		}
	}}

	return 0;
}
*/

