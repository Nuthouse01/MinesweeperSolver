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
extern bool RANDOM_USE_SMART_var;


// stats for a single game
struct game_stats {
	game_stats();
	void print_gamestats(int screen, class game * gameptr, class runinfo * runinfoptr);

	int strat_121, strat_nov_safe, strat_nov_flag; // number of times each MC strategy was used
	int num_guesses; // number of times it needed to do any type of guessing
	//records the phase transition history of solving the game, and the # of operations done in each phase
	// s#=single-cell, m#=multi-cell, O#=pre-smartguess optimization, W=win, X=lose, 
	// ^#=smartguess, r#=randomguess, z#=zeroguess, A#=chain-solver
	std::string trans_map;
	bool began_solving; // did I begin solving things, or did I lose before anything could be done?
	// TODO: if histogram is implemented and zerolist option is gone, this could be safely eliminated

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
};

// win/loss stats for a single program run
struct run_stats {
	run_stats();
	void print_final_stats(class runinfo * runinfoptr);
	void init_histogram(int num_mines);
	void inc_histogram(int minesplaced);
	void print_histogram(int numrows);

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
	int games_with_eights;

	float total_luck_in_wins; //
	float total_luck_in_losses; //
	float total_luck_per_guess;
};


#endif