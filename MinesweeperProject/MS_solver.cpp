// Brian Henson 7/4/2018
// this file contains the solver code/logic as implemented by whoever is writing it





// TODO: selectively remove these includes to whittle down what is actually needed/used in 'solver'
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


#include "MS_solver.h" // include myself
#include "MS_basegame.h"
#include "MS_settings.h"
#include "MS_stats.h"




// **************************************************************************************
// member functions for the buttload of structs

solutionobj::solutionobj(float ans, int howmany) {
	answer = ans; allocs = howmany;
	solution_flag = std::list<class cell *>(); // init as empty list
}


podwise_return::podwise_return() { // construct empty
	solutions = std::list<struct solutionobj>();
	effort = 0;
}
podwise_return::podwise_return(float ans, int howmany) { // construct from one value
	solutions = std::list<struct solutionobj>(1, solutionobj(ans, howmany));
	effort = 1;
}
// obvious
inline int podwise_return::size() { return solutions.size(); }
// returns value 0-1 showing what % of allocations were NOT tried (because they were found redundant), therefore higher efficiency is better
inline float podwise_return::efficiency() {
	float t1 = float(effort) / float(total_alloc());
	float t2 = 1. - t1;
	if (t2 > 0.) { return t2; } else { return 0.; }
}
// TODO: double-check that all 4 of these are actually live code!!
// podwise_return += list of cell pointers: append into all solutions
inline struct podwise_return& podwise_return::operator+=(const std::list<class cell *>& addlist) {
	for (std::list<struct solutionobj>::iterator listit = solutions.begin(); listit != solutions.end(); listit++) {
		listit->solution_flag.insert(listit->solution_flag.end(), addlist.begin(), addlist.end());
	}
	return *this;
}
// podwise_return *= int: multiply into all 'allocs' in the list
inline struct podwise_return& podwise_return::operator*=(const int& rhs) {
	for (std::list<struct solutionobj>::iterator listit = solutions.begin(); listit != solutions.end(); listit++) { listit->allocs *= rhs; }
	return *this;
}
// podwise_return += int: increment all 'answers' in the list
inline struct podwise_return& podwise_return::operator+=(const int& rhs) {
	for (std::list<struct solutionobj>::iterator listit = solutions.begin(); listit != solutions.end(); listit++) { listit->answer += rhs; }
	return *this;
}
// podwise_return += podwise_return: combine
inline struct podwise_return& podwise_return::operator+=(const struct podwise_return& rhs) {
	solutions.insert(solutions.end(), rhs.solutions.begin(), rhs.solutions.end());
	effort += rhs.effort;
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
	}
	return (a / float(total_weight));
}
// max: if there is a tie, or the solution doesn't represent only one allocations, return pointer to NULL
struct solutionobj * podwise_return::prmax() {
	std::list<struct solutionobj>::iterator solit = solutions.begin();
	std::list<struct solutionobj>::iterator maxit = solutions.begin(); // the one to return
	bool tied_for_max = false;
	for (solit++; solit != solutions.end(); solit++) {
		if (solit->answer < maxit->answer) { continue; } // existing max is greater
		if (solit->answer == maxit->answer) { tied_for_max = true; } // tied
		else if (solit->answer > maxit->answer) { tied_for_max = false; maxit = solit; }
	}
	if (tied_for_max || (maxit->allocs != 1)) { return NULL; } else { return &(*maxit); }
}
// max_val: if there is a tie, return a value anyway
float podwise_return::max_val() {
	float retmax = 0.;
	for (std::list<struct solutionobj>::iterator solit = solutions.begin(); solit != solutions.end(); solit++) {
		if (solit->answer <= retmax) { continue; } // existing max is greater
		if (solit->answer > retmax) { retmax = solit->answer; }
	}
	return retmax;
}
// min: if there is a tie, or the solution doesn't represent only one allocations, return pointer to NULL
struct solutionobj * podwise_return::prmin() {
	std::list<struct solutionobj>::iterator solit = solutions.begin();
	std::list<struct solutionobj>::iterator minit = solutions.begin(); // the one to return
	bool tied_for_min = false;
	for (solit++; solit != solutions.end(); solit++) {
		if (solit->answer > minit->answer) { continue; } // existing min is lesser
		if (solit->answer == minit->answer) { tied_for_min = true; } // tied
		else if (solit->answer < minit->answer) { tied_for_min = false; minit = solit;}
	}
	if (tied_for_min || (minit->allocs != 1)) { return NULL; } else { return &(*minit); }
}
// min_val: if there is a tie, return a value anyway
float podwise_return::min_val() {
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
	for (std::list<struct solutionobj>::iterator solit = solutions.begin(); solit != solutions.end(); solit++) {retval += solit->allocs;}
	return retval;
}




// shortcut to calculate the risk for a pod
float pod::risk() { return (100. * float(mines) / float(size())); }
// if outside recursion, return cell_list.size(); if inside recursion, return cell_list_size
int pod::size() { if (cell_list_size < 0) { return cell_list.size(); } else { return cell_list_size; } }
// scan the pod for any "special cases", perform 'switch' on result: 0=normal, 1=negative risk, 2=0 risk, 3=100 risk, 4=disjoint
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


// basic tuple-like constructor
riskreturn::riskreturn(float m, std::list<class cell *> * l) {
	minrisk = m; minlist = *l;
}

// basic tuple-like constructor
link_with_bool::link_with_bool(struct link asdf, bool f) {
	flagme = f; l = asdf;
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
// takes a cell pointer and adds a risk to its list... could modify to use x/y, but why bother?
void riskholder::addrisk(class cell * foo, float newrisk) {
	(riskarray[foo->x][foo->y]).riskvect.push_back(newrisk);
}
// iterate over itself and return the stuff tied for lowest risk
struct riskreturn riskholder::findminrisk() {
	std::list<class cell *> minlist = std::list<class cell *>();
	float minrisk = 100.;
	for (int m = 0; m < myruninfo.SIZEX_var; m++) {
		for (int n = 0; n < myruninfo.SIZEY_var; n++) {
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
	struct riskreturn retme = riskreturn(minrisk, &minlist);
	return retme;
}
// find the avg/max/min of the risks and return that value; also clear the list. if list is already empty, return -1
float riskholder::finalrisk(int x, int y) {
	if ((riskarray[x][y]).riskvect.empty())
		return -1.;
	float retval = 0.;
	int s = (riskarray[x][y]).riskvect.size();
	if (RISK_CALC_METHOD == 0) { // AVERAGE
		float sum = 0.;
		for (int i = 0; i < s; i++) {
			sum += (riskarray[x][y]).riskvect[i];
		}
		retval = sum / float(s);
	}
	if (RISK_CALC_METHOD == 1) { // MAXIMUM
		retval = -1.;
		for (int i = 0; i < s; i++) {
			float t = (riskarray[x][y]).riskvect[i];
			if (t > retval) { retval = t; }
		}
	}
	(riskarray[x][y]).riskvect.clear(); // clear it for use next time
	return retval;
}


// basic tuple-like constructor
scenario::scenario(std::list<std::list<struct link>::iterator> map, int num) {
	roadmap = map; allocs = num;
}






// **************************************************************************************
// sort/uniquify functions and general-use utility functions

// if a goes first, return negative; if b goes first, return positive; if identical, return 0
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
	if (compare_list_of_scenarios(a, b) < 0) { return true; } else { return false; }
}
bool equivalent_list_of_scenarios(std::list<std::list<struct link>::iterator> a, std::list<std::list<struct link>::iterator> b) {
	if (compare_list_of_scenarios(a, b) == 0) { return true; } else { return false; }
}






// find_nonoverlap: takes two vectors of cells, returns (first_vect_unique) (second_vect_unique) (overlap)
std::vector<std::vector<class cell *>> find_nonoverlap(std::vector<class cell *> me_unk, std::vector<class cell *> other_unk) {
	std::vector<class cell *> overlap = std::vector<class cell *>();
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
	std::vector<std::vector<class cell *>> retme;
	retme.push_back(me_unk);
	retme.push_back(other_unk);
	retme.push_back(overlap);
	return retme;
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
		buildme.push_back(std::vector<int>(K, -1)); // start a new entry, know it will have size K
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


// TODO: turn this into a templated thing (any type, vector or list) and move o basegame
// return a random cell from the provided list, or NULL if the list is empty
class cell * rand_from_list(std::list<class cell *> * fromme) {
	if ((*fromme).empty()) {
		return NULL;
	}
	int f = rand() % (*fromme).size();

	std::list<class cell *>::iterator iter = (*fromme).begin();
	for (int i = 0; i < f; i++) {
		iter++;
	}
	return *iter;
}



// **************************************************************************************

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



	static struct riskholder myriskholder;
	if (myriskholder.riskarray.empty()) { // because it's static, init it like this only once
		myriskholder = riskholder(myruninfo.SIZEX_var, myruninfo.SIZEY_var);
	}

	struct chain master_chain; // list of pods
	std::list<class cell *> interior_list = mygame.unklist;

	// step 1: iterate over field, get 'visible' cells, use them as roots to build pods.
	// non-optimized: includes duplicates, before pod-subtraction. interior_unk not yet modified.
	// pods are added to the chain already sorted (reading order by root), as are their cell_list contents
	for (int y = 0; y < myruninfo.SIZEY_var; y++) {
		for (int x = 0; x < myruninfo.SIZEX_var; x++) { // iterate over each cell
			if (mygame.field[x][y].get_status() == VISIBLE) {
				master_chain.podlist.push_back(pod(&mygame.field[x][y])); // constructor gets adj unks for the given root
			}
		}
	}

	// step 2: iterate over pods, check for dupes and subsets (call find_nonoverlap on each pod with root in 5x5 around my root)
	// note if a pod becomes 100% or 0%... repeat until no changes are done.
	// when deleting pods, what remains will still be in sorted order
	bool changes;
	do {
		changes = false;
		for (std::list<struct pod>::iterator podit = master_chain.podlist.begin(); podit != master_chain.podlist.end(); podit++) {
			// to find pods that may be dupes or subsets, use one of the following methods:
			for (int b = -2; b < 3; b++) {
				for (int a = -2; a < 3; a++) {
					if (((a == -2 || a == 2) && (b == -2 || b == 2)) || (a == 0 && b == 0)) { continue; } // skip myself and also the corners
					class cell * other = mygame.cellptr(podit->root->x + a, podit->root->y + b);
					if ((other == NULL) || (other->get_status() != VISIBLE)) { continue; } // only examine the visible cells
					std::list<struct pod>::iterator otherpod = master_chain.root_to_pod(other);
					if (otherpod == master_chain.podlist.end()) { continue; }
					// for each pod 'otherpod' with root within 5x5 found...
					std::vector<std::vector<class cell *>> n = find_nonoverlap(podit->cell_list, otherpod->cell_list);
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
				}
			}

		} // end for each pod
	} while (changes);

	// step 3: if any pods were found to be 100 or 0 when optimizing, just return them now
	if (!results.empty()) {
		if (ACTUAL_DEBUG) myprintfn(2, "DEBUG: in smart-guess, optimization found %i clear and %i flag\n", results.clearme.size(), results.flagme.size());
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
	if (!interior_list.empty() || (mygame.get_mines_remaining() <= RETURN_ACTUAL_ALLOCATION_IF_THIS_MANY_MINES_REMAIN)) {
		// step 5: iterate again, building links to anything within 5x5(only set MY links)
		for (std::list<struct pod>::iterator podit = master_chain.podlist.begin(); podit != master_chain.podlist.end(); podit++) {
			for (int b = -2; b < 3; b++) {
				for (int a = -2; a < 3; a++) {
					if (a == 0 && b == 0) { continue; } // skip myself BUT INCLUDE THE CORNERS
					class cell * other = mygame.cellptr(podit->root->x + a, podit->root->y + b);
					if ((other == NULL) || (other->get_status() != VISIBLE)) { continue; } // only examine the visible cells
					std::list<struct pod>::iterator otherpod = master_chain.root_to_pod(other);
					if (otherpod == master_chain.podlist.end()) { continue; } // some pods will have been optimized away
																			  // for each pod 'otherpod' with root within 5x5 found...
					std::vector<std::vector<class cell *>> n = find_nonoverlap(podit->cell_list, otherpod->cell_list);
					// ... only create links within podit!
					for (int i = 0; i < n[2].size(); i++) {
						podit->add_link(n[2][i], otherpod->root);
					}
				}
			}
		}

		// step 6: identify chains and sort the pods into a VECTOR of chains... 
		// at the same time, turn "cell_list" into a simple number, and clear the actual list so it uses less memory while recursing
		int numchains = master_chain.identify_chains();
		// if there are only a few mines left, then don't eliminate the cell_list contents
		bool reduce = (mygame.get_mines_remaining() > RETURN_ACTUAL_ALLOCATION_IF_THIS_MANY_MINES_REMAIN);
		std::vector<struct chain> listofchains = master_chain.sort_into_chains(numchains, reduce);

		// step 7: for each chain, recurse (depth, chain are only arguments) and get back list of answer allocations
		// handle the multiple podwise_retun objects, just sum their averages
		if (ACTUAL_DEBUG) myprintfn(2, "DEBUG: in smart-guess, # primary chains = %i \n", listofchains.size());
		float border_allocation = 0;
		std::vector<struct podwise_return> retholder = std::vector<struct podwise_return>(numchains, podwise_return());
		for (int s = 0; s < numchains; s++) {
			recursion_safety_valve = false; // reset the flag for each chain
			struct podwise_return asdf = podwise_recurse(0, listofchains[s]);
			border_allocation += asdf.avg();
			if (mygame.get_mines_remaining() <= RETURN_ACTUAL_ALLOCATION_IF_THIS_MANY_MINES_REMAIN) { retholder[s] = asdf; }
			if (ACTUAL_DEBUG || recursion_safety_valve) myprintfn(2, "DEBUG: in smart-guess, chain %i with %i pods found %i solutions\n", s, listofchains[s].podlist.size(), asdf.size());
			if (ACTUAL_DEBUG) myprintfn(2, "DEBUG: in smart-guess, chain %i eliminated %.3f%% of allocations as redundant\n", s, (100. * asdf.efficiency()));
			myrunstats.smartguess_valves_triggered += recursion_safety_valve;
		}


		// step 8: if near the endgame, check if any solutions fit perfectly
		if (mygame.get_mines_remaining() <= RETURN_ACTUAL_ALLOCATION_IF_THIS_MANY_MINES_REMAIN) {
			float minsum = 0; float maxsum = 0;
			for (int a = 0; a < numchains; a++) { minsum += retholder[a].min_val(); maxsum += retholder[a].max_val(); }

			if (minsum == mygame.get_mines_remaining()) {
				// int_list is safe
				results.clearme.insert(results.clearme.end(), interior_list.begin(), interior_list.end());
				// for each chain, get the minimum solution object (if there is only one), and if it has only 1 allocation, then apply it as flags
				for (int a = 0; a < numchains; a++) {
					struct solutionobj * minsol = retholder[a].prmin();
					if (minsol != NULL)
						results.flagme.insert(results.flagme.end(), minsol->solution_flag.begin(), minsol->solution_flag.end());
				}
			} else if ((maxsum + interior_list.size()) == mygame.get_mines_remaining()) {
				// int_list is all mines
				results.flagme.insert(results.flagme.end(), interior_list.begin(), interior_list.end());
				// for each chain, get the maximum solution object (if there is only one), and if it has only 1 allocation, then apply it
				for (int a = 0; a < numchains; a++) {
					struct solutionobj * maxsol = retholder[a].prmax();
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
			// TODO: calculate this info, but instead of just averaging it, export it to a separate file
			// solver progress / estimated mines / actual mines
			// somehow incorporate chain length? how hard is it to count the mines in each chain?
			// TODO: eventually implement a "bias function" to correct for mis-estimating, probably depend on whole game progress, chain size, estimated mines
			for (std::list<class cell *>::iterator cellit = interior_list.begin(); cellit != interior_list.end(); cellit++) {
				if ((*cellit)->value == MINE)
				interiormines++;
			}
			int bordermines = mygame.get_mines_remaining() - interiormines;

			if (ACTUAL_DEBUG) myprintfn(2, "DEBUG: in smart-guess, border_avg/ceiling/border_actual = %.3f / %i / %i\n", border_allocation, mygame.get_mines_remaining(), bordermines);
			myrunstats.smartguess_attempts++;
			myrunstats.smartguess_diff += (border_allocation - float(bordermines)); // accumulating a less-negative number
			////////////////////////////////////////////////////////////////////////


			if (border_allocation > mygame.get_mines_remaining()) { border_allocation = mygame.get_mines_remaining(); }
			interior_risk = (float(mygame.get_mines_remaining()) - border_allocation) / float(interior_list.size()) * 100.;
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
//		can have the recursion called on it (much faster this way). not sure what frequency it should rescan for chain integrity...
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
	if (rescan_counter < CHAIN_RECHECK_DEPTH) {
		rescan_counter++;
	} else {
		rescan_counter = 0;
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
		std::list<class cell *> cellsflaggedthislvl;
		struct chain copychain = mychain;
		struct podwise_return asdf;

		// these aren't "links", i'm just re-using the struct as a convenient way to couple
		// one link-cell cell with each pod it needs to be removed from
		std::list<struct link_with_bool> links_to_flag_or_clear;

		std::list<struct pod>::iterator frontpod = copychain.int_to_pod(mainpodindex);
		// step 3a: apply the changes according to the scenario by moving links from front() pod to links_to_flag_or_clear
		for (std::list<std::list<struct link>::iterator>::iterator r = scit->roadmap.begin(); r != scit->roadmap.end(); r++) {
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
		while (!links_to_flag_or_clear.empty()) {
			// for each link 'blinkit' to be flagged...
			struct link_with_bool blink = links_to_flag_or_clear.front();
			links_to_flag_or_clear.pop_front();

			class cell * sharedcell = blink.l.link_cell;
			bool tmp = false;
			// ...go to every pod with a root 'rootit' referenced in 'linkitf' and remove the link from them!
			for (std::list<class cell *>::iterator rootit = blink.l.linked_roots.begin(); rootit != blink.l.linked_roots.end(); rootit++) {
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
					if ((mygame.get_mines_remaining() <= RETURN_ACTUAL_ALLOCATION_IF_THIS_MANY_MINES_REMAIN) \
						&& (allocsthislvl == 1) && (activepod->mines > 0))
						cellsflaggedthislvl.insert(cellsflaggedthislvl.end(), activepod->cell_list.begin(), activepod->cell_list.end());
					copychain.podlist.erase(activepod);
					break; // not needed, but whatever
				}
			} // end of iteration on shared_roots
			if (tmp) {
				flagsthislvl++;
				if ((mygame.get_mines_remaining() <= RETURN_ACTUAL_ALLOCATION_IF_THIS_MANY_MINES_REMAIN) && (allocsthislvl == 1))
					cellsflaggedthislvl.push_back(sharedcell);
			}
		}// end of iteration on links_to_flag_or_clear

		 // step 4: recurse! when it returns, append to resultlist its value + how many flags placed in 3a/3b/3c
		asdf = podwise_recurse(rescan_counter, copychain);
		asdf += flagsthislvl; // inc the answer for each by how many flags placed this level
		asdf *= allocsthislvl; // multiply the # allocations for this scenario into each
		if ((mygame.get_mines_remaining() <= RETURN_ACTUAL_ALLOCATION_IF_THIS_MANY_MINES_REMAIN) && (allocsthislvl == 1))
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



// **************************************************************************************
// multi-cell strategies

// STRATEGY: 121 cross
// looks for a very specific arrangement of visible/unknown cells; the center is probably safe
// unlike the other strategies, this isn't based in logic so much... this is just a pattern I noticed
// return # cleared if it is applied, 0 otherwise, -1 if it unexpectedly lost
// NEW VERSION: don't place any flags, only reveal the center cell (it might help)
int strat_121_cross(class cell * center) {
	if (center->get_effective() != 2)
		return 0;
	std::vector<class cell *> adj = mygame.get_adjacent(center);
	if (adj.size() == 3) // must be in a corner
		return 0;

	int retme = 0;
	int r = 0;
	int s = 0;
	class cell * aa = mygame.cellptr((center->x) + 1, center->y);
	class cell * bb = mygame.cellptr((center->x) - 1, center->y);
	class cell * cc = mygame.cellptr(center->x, (center->y) + 1);
	class cell * dd = mygame.cellptr(center->x, (center->y) - 1);


	// assuming 121 in horizontal line:
	if ((aa != NULL) && (bb != NULL) && (aa->get_status() == VISIBLE) && (bb->get_status() == VISIBLE)
		&& (aa->get_effective() == 1) && (bb->get_effective() == 1)) {
		r = mygame.reveal(cc);
		s = mygame.reveal(dd);
		if ((r == -1) || (s == -1)) {
			return -1;
		} else {
			return (r + s);
		}
	}

	// assuming 121 in vertical line:
	if ((cc != NULL) && (dd != NULL) && (cc->get_status() == VISIBLE) && (dd->get_status() == VISIBLE)
		&& (cc->get_effective() == 1) && (dd->get_effective() == 1)) {
		r = mygame.reveal(aa);
		s = mygame.reveal(bb);
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
int strat_nonoverlap_safe(class cell * center) {
	if (center->get_effective() > 3)
		return false;
	std::vector<class cell *> me_unk = mygame.filter_adjacent(center, UNKNOWN);
	std::vector<class cell *> other_unk;
	int retme = 0;
	for (int b = -2; b < 3; b++) {
		for (int a = -2; a < 3; a++) {
			if (((a == -2 || a == 2) && (b == -2 || b == 2)) || (a == 0 && b == 0))
				continue; // skip myself and also the corners
			class cell * other = mygame.cellptr(center->x + a, center->y + b);
			if ((other == NULL) || (other->get_status() != VISIBLE))
				continue;
			if (center->get_effective() != other->get_effective()) // the two being compared must be equal
				continue;
			other_unk = mygame.filter_adjacent(other, UNKNOWN);
			if (me_unk.size() > other_unk.size())
				continue; //shortcut, saves time, nov-safe only

			std::vector<std::vector<class cell *>> nonoverlap = find_nonoverlap(me_unk, other_unk);
			// nov-safe needs to know the length of ME and the contents of OTHER
			if (nonoverlap[0].empty() && !(nonoverlap[1].empty())) {
				int retme_sub = 0;
				for (int i = 0; i < nonoverlap[1].size(); i++) {
					int r = mygame.reveal(nonoverlap[1][i]);
					if (r == -1) {
						return -1;
					}
					retme_sub += r;
				}
				retme += retme_sub; // increment by how many were cleared
				me_unk = mygame.filter_adjacent(center, UNKNOWN); // update me_unk, then continue iterating thru the 5x5
			}
		}
	}

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
int strat_nonoverlap_flag(class cell * center) {
	if ((center->get_effective() <= 1) || (center->get_effective() == 8)) // center must be 2-7
		return 0;
	//print_field(3);
	std::vector<class cell *> me_unk = mygame.filter_adjacent(center, UNKNOWN);
	std::vector<class cell *> other_unk;
	for (int b = -2; b < 3; b++) {
		for (int a = -2; a < 3; a++) {
			if (((a == -2 || a == 2) && (b == -2 || b == 2)) || (a == 0 && b == 0))
				continue; // skip myself and also the corners
			class cell * other = mygame.cellptr(center->x + a, center->y + b);
			if ((other == NULL) || (other->get_status() != VISIBLE))
				continue;
			// valid pairs are me/other: 2/1, 3/2, and 3/1; currently only know that me==2 or 3
			int z = 0;
			if ((other->get_effective() != 0) && (other->get_effective() < center->get_effective())) {
				z = center->get_effective() - other->get_effective();
			} else
				continue;
			other_unk = mygame.filter_adjacent(other, UNKNOWN);

			std::vector<std::vector<class cell *>> nonoverlap = find_nonoverlap(me_unk, other_unk);
			// nov-flag needs to know the length and contents of MY unique cells
			if (nonoverlap[0].size() == z) {
				int retval = 0;
				for (int i = 0; i < z; i++) {
					int r = mygame.set_flag(nonoverlap[0][i]);
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

	return 0;
}




