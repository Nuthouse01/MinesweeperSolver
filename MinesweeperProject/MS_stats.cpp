// Brian Henson 7/17/2018
// this file contains the structs for the stat-tracking code




#include "MS_settings.h"
#include "MS_basegame.h" // need this for myprintfn and print_field

#include "MS_stats.h" // include myself





// simple init
game_stats::game_stats() {
	trans_map = std::string();
	strat_121 = 0;
	strat_nov_safe = 0;
	strat_nov_flag = 0;
	num_guesses = 0;
	began_solving = false;
	smartguess_attempts = 0;
	smartguess_diff = 0.;
	smartguess_valves_tripped = 0;
	luck_value_mult = 1.; //
	luck_value_sum = 0.;
}

// simple init, also sets start time
run_stats::run_stats() {
	std::chrono::milliseconds ms = std::chrono::duration_cast< std::chrono::milliseconds >(
		std::chrono::system_clock::now().time_since_epoch()
		);
	start = ms.count();
	games_total = 0;
	games_won = 0;
	games_won_noguessing = 0;
	games_won_guessing = 0;
	games_lost = 0;
	games_lost_unexpectedly = 0;
	games_lost_beginning = 0; //
	games_lost_lategame = 0; //
	games_lost_midgame = 0; //
	games_lost_earlygame = 0; //
	strat_121_total = 0;
	strat_nov_safe_total = 0;
	strat_nov_flag_total = 0;
	num_guesses_in_wins = 0;
	num_guesses_in_losses = 0; //
	smartguess_attempts_total = 0;
	smartguess_diff_total = 0.;
	smartguess_valves_tripped_total = 0;
	games_with_eights = 0;
	total_luck_in_wins = 0.; //
	total_luck_in_losses = 0.; //
	total_luck_per_guess = 0.;

	game_loss_histogram.clear();
}


// prints everything nice and formatted; only happens once, so it could be in-line but this is better encapsulation
void run_stats::print_final_stats(class runinfo * runinfoptr) {
	// calculate total time elapsed and format for display
	std::chrono::milliseconds ms = std::chrono::duration_cast< std::chrono::milliseconds >(
		std::chrono::system_clock::now().time_since_epoch()
		);
	long f = ms.count();
	int elapsed_ms = int(f - start); // currently in ms
	double elapsed_sec = double(elapsed_ms) / 1000.; // seconds with a decimal

	double sec = fmod(elapsed_sec, 60.);
	int elapsed_min = int((elapsed_sec - sec) / 60.);
	int min = elapsed_min % 60;
	int hr = (elapsed_min - min) / 60;
	char timestr[20];
	sprintf_s(timestr, "%i:%02i:%06.3f", hr, min, sec);

	// print/log overall results (always print to terminal and log)
	myprintfn(2, "\n\nDone playing all %i games, displaying results! Time = %s\n\n", runinfoptr->NUM_GAMES, timestr);

	//if ((games_lost - game_loss_histogram[0]) > 50) {
		print_histogram(HISTOGRAM_RESOLUTION);
	//} else {
	//	myprintfn(2, "Histogram skipped because of small sample size\n\n");
	//}


	myprintfn(2, "MinesweeperSolver version %s\n", VERSION_STRING_def);
	myprintfn(2, "Games used X/Y/mines = %i/%i/%i, mine density = %4.1f%%\n", runinfoptr->get_SIZEX(), runinfoptr->get_SIZEY(), runinfoptr->get_NUM_MINES(),
		float(100. * float(runinfoptr->get_NUM_MINES()) / float(runinfoptr->get_SIZEX() * runinfoptr->get_SIZEY())));
	if (FIND_EARLY_ZEROS_var) {
		myprintfn(2, "Using 'hunting' method = succeed early (uncover only zeroes until solving can begin)\n");
	} else {
		myprintfn(2, "Used 'hunting' method = human-like (can lose at any stage)\n");
	}
	if (RANDOM_USE_SMART_var) {
		myprintfn(2, "Used 'guessing' mode = smartguess (slower but increased winrate)\n");
		myprintfn(2, "    Smartguess border est. avg deviation:   %+7.4f\n", (smartguess_diff_total / float(smartguess_attempts_total)));
		myprintfn(2, "    Times when recursion got out of hand:%5i\n", smartguess_valves_tripped_total);
	} else {
		myprintfn(2, "Used 'guessing' mode = guess randomly (lower winrate but faster)\n");
	}
	myprintfn(2, "Average time per game:                     %8.4f sec\n", (float(elapsed_sec) / float(games_total)));
	myprintfn(2, "Average 121-cross uses per game:           %5.1f\n", (float(strat_121_total) / float(games_total)));
	myprintfn(2, "Avg nonoverlap-flag (simple) per game:     %5.1f\n", (float(strat_nov_flag_total) / float(games_total)));
	myprintfn(2, "Avg nonoverlap-safe (simple) per game:     %5.1f\n", (float(strat_nov_safe_total) / float(games_total)));
	myprintfn(2, "Avg number of guesses needed to win:       %6.2f + initial guess\n", (float(num_guesses_in_wins) / float(games_won)));
	myprintfn(2, "Average safety per guess:                  %7.3f%%\n", 100. * (float(total_luck_per_guess) / float(games_total + num_guesses_in_losses + num_guesses_in_wins))); // everything
	//myprintfn(2, "Avg luck/safety value in each win:         %7.3f%%\n", 100. * (float(total_luck_in_wins) / float(games_won)));

	// TODO: add 'avg risk/safety for each guess' ?
	// (1. - (float(runinfoptr->get_NUM_MINES()) / float(runinfoptr->get_SIZEX() * runinfoptr->get_SIZEY())));
	// average luck per guess:
	// (sum of luck from all guesses) / (num guesses in wins + num guesses in losses)
	// (sum of luck from all guesses + initial) / (num guesses in wins + num guesses in losses + total games)
	if (games_with_eights != 0) {
	myprintfn(2, "    Games with 8-adj cells:              %5i\n", games_with_eights);
	}
	myprintfn(2, "\n");
	myprintfn(2, "Total games played:                     %6i\n", games_total);

	myprintfn(2, "    Total games won:                     %5i   %5.1f%%    -----\n", games_won, (100. * float(games_won) / float(games_total)));
	myprintfn(2, "        Games won without guessing:      %5i   %5.1f%%   %5.1f%%\n", games_won_noguessing, (100. * float(games_won_noguessing) / float(games_total)), (100. * float(games_won_noguessing) / float(games_won)));
	myprintfn(2, "        Games won that required guessing:%5i   %5.1f%%   %5.1f%%\n", games_won_guessing, (100. * float(games_won_guessing) / float(games_total)), (100. * float(games_won_guessing) / float(games_won)));
	myprintfn(2, "    Total games lost:                    %5i   %5.1f%%    -----\n", games_lost, (100. * float(games_lost) / float(games_total)));
	myprintfn(2, "        Games lost before 1rst flag(<1%%):%5i   %5.1f%%   %5.1f%%\n", game_loss_histogram[0], (100. * float(game_loss_histogram[0]) / float(games_total)), (100. * float(game_loss_histogram[0]) / float(games_lost)));
	myprintfn(2, "        Games lost while solving(1-99%%): %5i   %5.1f%%   %5.1f%%\n", games_lost - game_loss_histogram[0], (100. * float(games_lost - game_loss_histogram[0]) / float(games_total)), (100. * float(games_lost - game_loss_histogram[0]) / float(games_lost)));
	// since I have the histogram I don't need to break it down further than "in the histogram vs not"
	if (games_lost_unexpectedly != 0) {
	myprintfn(2, "        Games lost unexpectedly:         %5i   %5.1f%%   %5.1f%%\n", games_lost_unexpectedly, (100. * float(games_lost_unexpectedly) / float(games_total)), (100. * float(games_lost_unexpectedly) / float(games_lost)));
	}
	myprintfn(2, "\n");

	fflush(runinfoptr->logfile);
}



// print the stats of the current game, and some whitespace below
// if SCREEN=0, don't print anything. if SCREEN=1, print to log. if SCREEN=2, print to both.
void game_stats::print_gamestats(int screen, class game * gameptr, class runinfo * runinfoptr) {
	gameptr->print_field(3, screen);
	myprintfn(screen, "Transition map: %s\n", trans_map.c_str());
	myprintfn(screen, "121-cross hits: %i, nonoverlap-safe hits: %i, nonoverlap-flag hits: %i\n",
		strat_121, strat_nov_safe, strat_nov_flag);
	myprintfn(screen, "Cells guessed: %i\n", num_guesses);
	myprintfn(screen, "Chance of safely getting this far: %.4f%%\n", 100. * luck_value_mult);
	myprintfn(screen, "Flags placed: %i / %i\n\n\n", runinfoptr->get_NUM_MINES() - gameptr->get_mines_remaining(), runinfoptr->get_NUM_MINES());
	fflush(runinfoptr->logfile);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// once number of mines is known, set up the histogram
void run_stats::init_histogram(int num_mines) {
	game_loss_histogram.clear();
	game_loss_histogram.resize(num_mines, 0);
	game_loss_histogram.shrink_to_fit();
}
// increment the correct entry
void run_stats::inc_histogram(int minesplaced) {
	game_loss_histogram[minesplaced]++;
}
// print a bar graph of the losses, configurable resolution
// excludes any "first move" losses, game_loss_histogram[0]
// NOTE: when displayed, bars will be horizontal rows, but when talking about it, I will picture them as vertical columns
// I will also lose resolution when it is printed with ASCII, but can't do anything about that
void run_stats::print_histogram(int numrows) {
	// 1: find the borders between bars
	int s = game_loss_histogram.size() - 1; // s=89
	if (numrows > s) { numrows = s; }
	float barwidth = float(s) / float(numrows);
	// bars borders are compared against the halfpoints to interpolate; therefore endpoints must be 0.5 and 89.5
	std::vector<float> border = std::vector<float>(numrows, 0); // contains the right endpoint for the range of each bar
	float b = 0.5;
	for (int i = 0; i < numrows - 1; i++) { b += barwidth; border[i] = b; }
	border[numrows - 1] = float(s) + 0.6; // too big so it will never be determined 'less than' the end
	// 'borders' now holds all right-end borderpoints; ends are known to be 0.5 and s+0.5


	// 2: make a new histogram and put the old histogram's contents into it
	// when losing resolution horizontally (from 89 entries to 10), must interpolate to prevent spikes from forming
	// if barwidth = 2.2, it could range from 7.9 to 10.1 (containing 8/9/10) and then from 10.1 to 12.3 (containing 11/12)
	// to solve this, border zones must be split between the buckets they're going into
	std::vector<float> new_histogram = std::vector<float>(numrows, 0);
	int q = 0; // index of borders to be comparing against, and new_histogram bucket to put numbers into
	for (int i = 1; i < game_loss_histogram.size(); i++) {
		// is this entry in the middle of a bar, or does it need split?
		if (border[q] < (float(i) + 0.5)) {
			// i needs splitting; part goes into q, part goes into q+1
			float z = border[q] - (float(i) - 0.5); // z is [0-1]
			new_histogram[q]   += float(game_loss_histogram[i]) * z;
			new_histogram[q+1] += float(game_loss_histogram[i]) * (1. - z);
			q++;
		} else {
			// normal case:
			new_histogram[q] += float(game_loss_histogram[i]);
		}
	}
	// new_histogram has now been filled


	// 3: actually display it
	// uses HISTOGRAM_RESOLUTION horizontal bars, scaled so that the largest bar is HISTOGRAM_MAX_HORIZ_SIZE chars
	// display actual number to the left
	// doesn't have good vertical axis labels tho, and horizontal axis is relative.

	// find max of the new histogram:
	float z = 0;
	for (int i = 0; i < new_histogram.size(); i++) {
		if (new_histogram[i] > z) z = new_histogram[i];
	}

	if (z != 0) {
		// HORIZONTAL DISPLAY:
		myprintfn(2, "Distribution of when losses occur (in terms of game completion):\n");
		myprintfn(2, " # games | 1%% of flags placed\n");
		// for-loop
		for (int i = 0; i < new_histogram.size(); i++) {
			std::string row = std::string(int(float(HISTOGRAM_MAX_HORIZ_SIZE) * new_histogram[i] / z), '#');
			myprintfn(2, "%7.0f  |%s\n", new_histogram[i], row.c_str());
		}
		myprintfn(2, "         | 99%% of flags placed\n\n");
	}
}

