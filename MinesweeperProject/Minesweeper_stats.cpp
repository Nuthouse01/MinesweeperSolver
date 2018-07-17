// Brian Henson 7/17/2018
// this file contains the structs for the stat-tracking code




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


#include "Minesweeper_stats.h" // include myself
#include "Minesweeper_settings.h"
#include "Minesweeper_basegame.h" // need this for myprintfn and print_field






// simple init
game_stats::game_stats() {
	trans_map = std::string();
	strat_121 = 0;
	strat_nov_safe = 0;
	strat_nov_flag = 0;
	times_guessing = 0;
	began_solving = false;
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
	games_lost_beginning = 0;
	games_lost_unexpectedly = 0;
	games_lost_lategame = 0;
	games_lost_midgame = 0;
	games_lost_earlygame = 0;
	strat_121_total = 0;
	strat_nov_safe_total = 0;
	strat_nov_flag_total = 0;
	smartguess_attempts = 0;
	smartguess_diff = 0.;
	smartguess_valves_triggered = 0;
	games_with_eights = 0;
	num_guesses_total = 0;
}





// prints everything nice and formatted; only happens once, so it could be in-line but this is better encapsulation
void run_stats::print_final_stats() {
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
	myprintfn(2, "\nDone playing all %i games, displaying results! Time = %s\n\n", myruninfo.NUM_GAMES_var, timestr);
	myprintfn(2, "MinesweeperSolver version %s\n", VERSION_STRING_def);
	myprintfn(2, "Games used X/Y/mines = %i/%i/%i, mine density = %4.1f%%\n", myruninfo.SIZEX_var, myruninfo.SIZEY_var, myruninfo.NUM_MINES_var,
		float(100. * float(myruninfo.NUM_MINES_var) / float(myruninfo.SIZEX_var * myruninfo.SIZEY_var)));
	if (FIND_EARLY_ZEROS_var) {
		myprintfn(2, "Using 'hunting' method = succeed early (uncover only zeroes until solving can begin)\n");
	} else {
		myprintfn(2, "Used 'hunting' method = human-like (can lose at any stage)\n");
	}
	if (RANDOM_USE_SMART_var) {
		myprintfn(2, "Used 'guessing' mode = smartguess (slower but increased winrate)\n");
		myprintfn(2, "    Smartguess border est. avg deviation:   %+7.4f\n", (smartguess_diff / float(smartguess_attempts)));
		myprintfn(2, "    Times when recursion got out of hand:%5i\n", smartguess_valves_triggered);
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

	fflush(myruninfo.logfile);
}




// print the stats of the current game, and some whitespace below
// if SCREEN=0, don't print anything. if SCREEN=1, print to log. if SCREEN=2, print to both.
void game_stats::print_gamestats(int screen) {
	mygame.print_field(3, screen);
	myprintfn(screen, "Transition map: %s\n", trans_map.c_str());
	myprintfn(screen, "121-cross hits: %i, nonoverlap-safe hits: %i, nonoverlap-flag hits: %i\n",
		strat_121, strat_nov_safe, strat_nov_flag);
	myprintfn(screen, "Cells guessed: %i\n", times_guessing);
	myprintfn(screen, "Flags placed: %i / %i\n\n\n", myruninfo.NUM_MINES_var - mygame.get_mines_remaining(), myruninfo.NUM_MINES_var);
	fflush(myruninfo.logfile);
}


