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
	began_solving = false; //
	smartguess_attempts = 0;
	smartguess_diff = 0.;
	smartguess_valves_tripped = 0;
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
	num_guesses_total = 0;
	smartguess_attempts_total = 0;
	smartguess_diff_total = 0.;
	smartguess_valves_tripped_total = 0;
	games_with_eights = 0;

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
	myprintfn(2, "Games used X/Y/mines = %i/%i/%i, mine density = %4.1f%%\n", runinfoptr->SIZEX, runinfoptr->SIZEY, runinfoptr->NUM_MINES,
		float(100. * float(runinfoptr->NUM_MINES) / float(runinfoptr->SIZEX * runinfoptr->SIZEY)));
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
	myprintfn(2, "Average nonoverlap-flag uses per game:     %5.1f\n", (float(strat_nov_flag_total) / float(games_total)));
	myprintfn(2, "Average nonoverlap-safe uses per game:     %5.1f\n", (float(strat_nov_safe_total) / float(games_total)));
	myprintfn(2, "Average number of guesses per game:        %6.2f + initial guess\n\n", (float(num_guesses_total) / float(games_total)));
	myprintfn(2, "Total games played:                     %6i\n", games_total);

	if (games_with_eights != 0) {
		myprintfn(2, "    Games with 8-adj cells:              %5i\n", games_with_eights);
	}
	myprintfn(2, "    Total games won:                     %5i   %5.1f%%    -----\n", games_won, (100. * float(games_won) / float(games_total)));
	myprintfn(2, "        Games won without guessing:      %5i   %5.1f%%   %5.1f%%\n", games_won_noguessing, (100. * float(games_won_noguessing) / float(games_total)), (100. * float(games_won_noguessing) / float(games_won)));
	myprintfn(2, "        Games won that required guessing:%5i   %5.1f%%   %5.1f%%\n", games_won_guessing, (100. * float(games_won_guessing) / float(games_total)), (100. * float(games_won_guessing) / float(games_won)));
	myprintfn(2, "    Total games lost:                    %5i   %5.1f%%    -----\n", games_lost, (100. * float(games_lost) / float(games_total)));
	myprintfn(2, "        Games lost in the first move(s): %5i   %5.1f%%   %5.1f%%\n", games_lost_beginning, (100. * float(games_lost_beginning) / float(games_total)), (100. * float(games_lost_beginning) / float(games_lost)));
	myprintfn(2, "        Games lost early      (1-15%%):   %5i   %5.1f%%   %5.1f%%\n", games_lost_earlygame - games_lost_beginning, (100. * float(games_lost_earlygame - games_lost_beginning) / float(games_total)), (100. * float(games_lost_earlygame - games_lost_beginning) / float(games_lost)));
	myprintfn(2, "        Games lost in midgame (15-85%%):  %5i   %5.1f%%   %5.1f%%\n", games_lost_midgame, (100. * float(games_lost_midgame) / float(games_total)), (100. * float(games_lost_midgame) / float(games_lost)));
	myprintfn(2, "        Games lost in lategame(85-99%%):  %5i   %5.1f%%   %5.1f%%\n", games_lost_lategame, (100. * float(games_lost_lategame) / float(games_total)), (100. * float(games_lost_lategame) / float(games_lost)));
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
	myprintfn(screen, "Flags placed: %i / %i\n\n\n", runinfoptr->NUM_MINES - gameptr->get_mines_remaining(), runinfoptr->NUM_MINES);
	// TODO: create and also print "luck value"
	fflush(runinfoptr->logfile);
}


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
// print a bar graph of the losses, 10? rows
// excludes any "first move" losses, game_loss_histogram[0]
// NOTE: when displayed, bars will be horizontal rows, (TODO: or will it?) but when talking about it, I will picture them as vertical columns
// when losing resolution horizontally (from 89 entries to 10), must interpolate to prevent spikes from forming
// if barwidth = 2.2, it could range from 7.9 to 10.1 (containing 8/9/10) and then from 10.1 to 12.3 (containing 11/12)
// to solve this, border zones must be split between the categories they're going into
// I will also lose resolution vertically when it is printed with ASCII, but can't do anything about that
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
	// some sort of auto-scaling? is max height 100% or the highest of the histogram entries? WHAT SIZE is max height?
	// do I want to print it horizontal or vertical? VERY different formatting strategy
	// probably want to put the actual value next to the bar (rounded down)... easy for rows, hard for columns
	// decided: max size is 40 * '#' horizontal, for the biggest of the values

	// find max of the new histogram:
	float z = 0;
	for (int i = 0; i < new_histogram.size(); i++) {
		if (new_histogram[i] > z) z = new_histogram[i];
	}

	// HORIZONTAL DISPLAY:
	//myprintfn(2, "Total number of in-progress losses: %i\n", games_lost - game_loss_histogram[0]);
	myprintfn(2, "Distribution of when losses occur (in terms of game completion):\n");
	myprintfn(2, "num games| 1%% of flags placed\n");
	// for-loop
	for (int i = 0; i < new_histogram.size(); i++) {
		std::string row = std::string(int(float(HISTOGRAM_MAX_HORIZ_SIZE) * new_histogram[i] / z), '#');
		myprintfn(2, "%7.0f  |%s\n", new_histogram[i], row.c_str());
	}
	myprintfn(2, "         | 99%% of flags placed\n\n");
}







