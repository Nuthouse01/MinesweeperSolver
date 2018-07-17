#ifndef MS_STATS
#define MS_STATS
// Brian Henson 7/17/2018
// this file contains the structs for the stat-tracking code




extern class runinfo myruninfo;
extern class game mygame;
extern bool FIND_EARLY_ZEROS_var;
extern bool RANDOM_USE_SMART_var;


// stats for a single game
struct game_stats {
	game_stats();
	int strat_121, strat_nov_safe, strat_nov_flag; // number of times each MC strategy was used
	int times_guessing; // number of times it needed to do hunting or guessing
	//records the phase transition history of solving the game, and the # of operations done in each phase
	// ^#=hunting, G#=guessing, s#=single-cell, m#=multi-cell, A=advanced
	std::string trans_map;
	bool began_solving; // did I begin solving things, or did I lose before anything could be done?
	// TODO: if histogram is implemented and zerolist option is gone, this could be safely eliminated

	void print_gamestats(int screen);
};

// win/loss stats for a single program run
struct run_stats {
	run_stats();

	void print_final_stats();
	long start;
	int games_total;			// total games played to conclusion (not really needed but w/e)

	int games_won;				// total games won
	int games_won_noguessing;	// games won without "end-game guessing"
	int games_won_guessing;		// games won by "end-game guessing"

	int games_lost;				// total games lost
	int games_lost_beginning;	// games lost before any logic could be applied (0 when hunting correctly)
	int games_lost_earlygame;	// 0-15% completed
	int games_lost_midgame;		// 15-85% completed
	int games_lost_lategame;	// 85-99% completed
	int games_lost_unexpectedly;// losses from other situations (should be 0)

	int strat_121_total;
	int strat_nov_safe_total;
	int strat_nov_flag_total;
	int num_guesses_total;

	int smartguess_attempts;
	float smartguess_diff;
	int smartguess_valves_triggered;
	int games_with_eights;
};






#endif