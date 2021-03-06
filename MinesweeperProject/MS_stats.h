#ifndef MS_STATS
#define MS_STATS
// Brian Henson 7/17/2018
// this file contains the structs for the stat-tracking code



#include <cstdlib> // rand, other stuff
#include <cstdio> // printf
#include <string> // for printing the histogram
#include <vector> // used for histogram
#include <chrono> // just because time(0) only has 1-second resolution



// didn't wanna extern anything but whatever
extern bool FIND_EARLY_ZEROS_var;
extern int GUESSING_MODE_var;


// stats for a single game
struct game_stats {
	// simple init
	game_stats();

	int strat_121, strat_nov_safe, strat_nov_flag; // number of times each MC strategy was used
	int num_guesses; // number of times it needed to do any type of guessing
	std::string trans_map;	//records the phase transition history of solving the game, and the # of operations done in each phase
							// s#=single-cell, t#=two-cell, M#=pre-smartguess multicell logic, W=win, X=lose, 
							// ^#=smartguess, r#=randomguess, z#=zeroguess, A#=chain-solver
	int smartguess_attempts;
	float smartguess_diff;
	int smartguess_valves_tripped;
	float luck_value_mult; //
	float luck_value_sum;
	// every time a guess is made, multiply into this the chance that the guess is SAFE
	// NOTE: do I want to take into account the initial guess? probably...
	// starts at 1, if I guess a 1/3 = 33% risk = 67% safe, then luck *= .67
	// if =.67, if I guess another 1/3 then luck *= .67
	// if =.44, if i guess a 1/2 = 50% risk = 50% safe, then luck *= .5, would be .22
	// not yet sure how I want to display it tho

	// print the stats of the current game, and some whitespace below. SCREEN=0, don't print anything. SCREEN=1, print to log. SCREEN=2, print to both.
	void print_gamestats(int screen, class game * gameptr, class runinfo * runinfoptr);
};

// win/loss stats for a single program run
struct run_stats {
	// simple init, also sets start time
	run_stats();

	long start;
	int games_total;			// total games played to conclusion (not really needed but w/e)

	int games_won;				// total games won
	int games_won_noguessing;	// games won without "end-game guessing"
	int games_won_guessing;		// games won by "end-game guessing"

	int games_lost;				// total games lost
	int games_lost_beginning;	// // games lost before any logic could be applied (0 when hunting correctly)
	int games_lost_earlygame;	// // 0-15% completed
	int games_lost_midgame;		// // 15-85% completed
	int games_lost_lategame;	// // 85-99% completed
	int games_lost_unexpectedly;// losses from other situations (should be 0)

	std::vector<int> game_loss_histogram;
	// histogram: tracks how many games were lost with 1 mine remaining, 2 mines remaining, 3 mines remaining, etc
	// but instead it's reversed, and counts how many games lost after placing 0 mines, 1 mine, 2 mines, etc (vector indices)

	int strat_121_total;
	int strat_nov_safe_total;
	int strat_nov_flag_total;
	int num_guesses_in_wins;
	int num_guesses_in_losses;

	int smartguess_attempts_total;
	float smartguess_diff_total;
	int smartguess_valves_tripped_total;
	int games_with_smartguess_valves_tripped;
	int games_with_eights;

	float total_luck_in_wins; //
	float total_luck_in_losses; //
	float total_luck_per_guess;

	// prints everything nice and formatted; only happens once, so it could be in-line but this is better encapsulation
	void print_final_stats(class runinfo * runinfoptr);
	// once number of mines is known, set up the histogram
	void init_histogram(int num_mines);
	// increment the correct entry. could be in-line but this is better encapsulation
	void inc_histogram(int minesplaced);
	// print a bar graph of the losses, configurable resolution
	void print_histogram(int numrows);
};


#endif