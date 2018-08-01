/* 
MinesweeperProject.cpp
Brian Henson, I forget when I started but it was like April or something
This whole program (and all solver logic within it) was developed all on my own, without looking up anything
online. I'm sure someone else has done their PHD thesis on creating the 'perfect minesweeper solver' but I
didn't look at any of their work, I developed these strategies all on my own.

Contents: 
verhist.txt, readme.txt, MS_MAIN.cpp, MS_settings.h
MS_basegame.cpp, MS_basegame.h
MS_stats.cpp, MS_stats.h
MS_solver.cpp, MS_solver.h
*/

// NOTE: v4.4, moved version history to verhist.txt because VisualStudio is surprisingly awful at scanning long block-comments


// testing
// TODO: test smartguess algorithm winrate with border risk determined via max/min/average of the various contributing pods
//		^ results are that min is bad, avg/max are roughly equal
// TODO: test smartguess algorithm speed/accuracy with choosing a pod in the middle (rand), or in the 'front'
// TODO: re-test 'weighted avg' option for speed/accuracy/winrate now that the rest of the algorithm works good, 100k games
//		^ results inconclusive, unweighted has slight improvement maybe? 18.4 vs 18.6, within range of uncertainty
// TODO: re-test algorithm speed/accuracy with different chain recheck level values

// possibly hard
// TODO: let smartguess find and apply only the concrete parts of a potential solution within a chain? IDEA: apply any flags that are shared
//		by ALL min or max solutions, when there are multiple min or max solutions?
// TODO: maybe have smartguess give preference to cells nearer the border, as a tie-breaker? if the risk is the same, might as well
//		go for something with a greater reward

// very minor or no benefits
// TODO: change the logfile print method to instead use >> so if it unexpectedly dies I still have a partial logfile, and/or can read it
//		while the code is running? very anoying how 'release' mode optimizes away the fflush command
// TODO: determine histogram of recursion depth overall, and related to # of pods or # of mines remaining
// TODO: use Matlab or something to run the program many many many many times, with different field parameters, and correlate winrate
//		with field size or mine density or something. BUILD GRAPHS!



// observations
// Originally I thought winrate would depend only on mine density, but no. Density determines the "inherent risk" or "base risk"
// from making a guess, whether smartguess or not; greater density = greater base risk. But, the total # of mines (or the total field
// size) also is important, because that determines the length of the game; longer games = more guesses needed to complete. Doubling
// the size and mines (so density stays the same) reduces winrate drastically, perhaps similar to asking the player to win two games
// in a row.


// not sure what this is but Visual Studio wants me to have it
#include <SDKDDKVer.h> 
#define _USE_MATH_DEFINES
#include <math.h>


#include <cstdlib> // rand, other stuff
#include <cstdio> // printf, file pointer (probably)
#include <iostream> // cin and getline for input parsing
#include <string> // parsing, logfile
#include <vector> // used for handling lists of cells in play_game
#include <cassert> // so it aborts when something wierd happens
#include <ctime> // logfile timestamp
#include <chrono> // used to seed the RNG because time(0) only has 1-second resolution
#include <random> // because I don't like using sequential seeds, or waiting when going very fast
#include <Windows.h> // needed to test for and create LOGS directory
// TODO: Windows.h adds the actual max() and min() macros, maybe scan the code and see if I can put them in somewhere


#include "MS_settings.h"
#include "MS_basegame.h"
#include "MS_stats.h"
#include "MS_solver.h"




/////////////////////////////////////////////////////////////////////////////////////////////////



// global vars:
// these might like to go in the myruninfo struct, but they're intrinsic to the solver, not the game, so I left them isolated
bool FIND_EARLY_ZEROS_var = false; // may want to eliminate this option so i can hide zerolist and get perfect privacy enforcement???
bool RANDOM_USE_SMART_var = false;

class game mygame = game();					// init empty, will fill the field_blank later
class runinfo myruninfo = runinfo();		// init emtpy, will fill during input parsing
struct run_stats myrunstats = run_stats();	// init empty, will set up histogram later
struct game_stats mygamestats; // don't explicitly init here, its reset on each loop



// major structural functions (just for encapsulation, each is only called once)
inline int parse_input_args(int margc, char *margv[]);
inline int play_game();



// ********************************************************************************************

// takes argc and argv, processes input args, runs interactive prompt if appropriate
// return 0 on success, 1 if something makes me want to abort
// values are stored into global variables that replace the #define statements
// WARNING: the idiot-proofing is very weak here; guaranteed not to crash, but will interpret non-numberic values as 0s
inline int parse_input_args(int argc, char *argv[]) {

// helptext:
const char helptext[1700] = "MinesweeperSolver v3.0 by Brian Henson\n\
This program is intended to generate and play a large number of Minesweeper\n\
games to collect win/loss info or whatever other data I feel like. It applies\n\
single-cell and multi-cell logical strategies as much as possible before\n\
revealing any unknown cells,of course. An extensive log is generated showing\n\
most of the stages of the solver algorithm. Each game is replayable by using the\n\
corresponding seed in the log, for closer analysis or debugging.\n\n\
*Usage/args:\n\
   -h, -?:               Print this text, then exit\n\
   -pro, -prompt:        Interactively enter various run/game settings.\n\
       Automatically chosen when no args are given.\n\
   -def, -default:       Run the program using default values\n\
*To apply settings from the command-line, use any number of these:\n\
   -num, -numgames:      How many games to play with these settings\n\
   -field:               Field size and number of mines, format= #x-#y-#mines\n\
   -findz, -findzero:    1=on, 0=off. If on, reveal zeroes during earlygame.\n\
       Not human-like but usually reaches end-game.\n\
   -smart, -smartguess:  1=on, 0=off. Replaces random guessing method with\n\
       speculative allocation of mines and risk calculation. Increases runtime\n\
       by 2-8x(avg) but increases winrate.\n\
   -seed:                0=random seed, other=specify seed. Suppresses -num \n\
       argument and plays only 1 game.\n\
   -scr, -screen:        How much printed to screen. 0=minimal clutter,\n\
       1=results for each game, 2=everything\n\n";
   


	// apply the defaults
	//myruninfo.SIZEX = SIZEX_def;
	int tempsizex = SIZEX_def;
	//myruninfo.SIZEY = SIZEY_def; 
	int tempsizey = SIZEY_def;
	//myruninfo.NUM_MINES = NUM_MINES_def;
	int tempnummines = NUM_MINES_def;
	myruninfo.NUM_GAMES = NUM_GAMES_def;
	myruninfo.SPECIFY_SEED = SPECIFY_SEED_def;
	myruninfo.SCREEN = SCREEN_def;
	FIND_EARLY_ZEROS_var = FIND_EARLY_ZEROS_def;
	RANDOM_USE_SMART_var = RANDOM_USE_SMART_def;


	/*
	behavior: with no args, or with 'prompt' arg, interactively prompt the user
	help: print a block of text and exit
	also have 'default' arg to run with defaults
	if not 'default' or 'prompt' or empty, then args set/overwrite the default values... doesn't require all to be specified
	*/

	if (argc == 1) {
		// no arguments specified, do the interactive prompt thing
	LABEL_PROMPT:
		std::string bufstr;
		printf_s("Please enter settings for running the program. Defaults values are in brackets.\n");
		printf_s("Number of games: [%i]  ", myruninfo.NUM_GAMES);
		std::getline(std::cin, bufstr);
		if (bufstr.size() != 0) {
			// convert and apply
			myruninfo.NUM_GAMES = atoi(bufstr.c_str());
			// NOTE: if the input is not numeric, it simply returns 0 instead of complaining
			if (myruninfo.NUM_GAMES < 1) {
				printf_s("ERR: #games cannot be zero or negative\n"); return 1;
			}
		}

		printf_s("Field type, format X-Y-mines: [%i-%i-%i]  ", tempsizex, tempsizey, tempnummines);
		std::getline(std::cin, bufstr);
		if (bufstr.size() != 0) {
			int f = 0; // the index currently examined
			int s = 0; // the place to store the next index
			int indices[5];
			while (f < bufstr.size()) {
				if (bufstr[f] == '-') { indices[s] = f; s++; }
				f++;
			}
			if (s != 2) {
				printf_s("ERR: gamestring must have format '#-#-#', '%s' is unacceptable\n", bufstr.c_str()); return 1;
			}

			// read the values and convert them to ints, then store them
			std::string temp;
			temp = bufstr.substr(0, indices[0]);
			tempsizex = atoi(temp.c_str());
			temp = bufstr.substr(indices[0] + 1, indices[1] - (indices[0] + 1));
			tempsizey = atoi(temp.c_str());
			temp = bufstr.substr(indices[1] + 1); // from here to the end
			tempnummines = atoi(temp.c_str());

			// error checking
			if (tempsizex < 1) {printf_s("ERR: sizeX cannot be zero or negative\n"); return 1;}
			if (tempsizey < 1) {printf_s("ERR: sizeY cannot be zero or negative\n"); return 1;}
			if (tempnummines > (tempsizex * tempsizey)) {printf_s("ERR: more mines than squares in the field!\n"); return 1;}
			if (tempnummines < 1) {printf_s("ERR: #mines cannot be zero or negative\n"); return 1;}
		}


		printf_s("Always reveal zeros earlygame, 0/1: [%i]  ", FIND_EARLY_ZEROS_var);
		std::getline(std::cin, bufstr);
		if (bufstr.size() != 0) {
			// convert and apply
			FIND_EARLY_ZEROS_var = bool(atoi(bufstr.c_str()));
			// NOTE: if the input is not numeric, it simply returns 0 instead of complaining
		}

		printf_s("Use smarter guessing, 0/1: [%i]  ", RANDOM_USE_SMART_var);
		std::getline(std::cin, bufstr);
		if (bufstr.size() != 0) {
			// convert and apply
			RANDOM_USE_SMART_var = atoi(bufstr.c_str());
			// NOTE: if the input is not numeric, it simply returns 0 instead of complaining
		}

		// NOTE: decided that specifying a seed is used so rarely, it doesn't need to be prompted; -seed only is enough
		//printf_s("Specify seed to use, 0=random: [%i]  ", SPECIFY_SEED);
		//std::getline(std::cin, bufstr);
		//if (bufstr.size() != 0) {
		//	// convert and apply
		//	SPECIFY_SEED = atoi(bufstr.c_str());
		//	// NOTE: if the input is not numeric, it simply returns 0 instead of complaining
		//	//if (SPECIFY_SEED < 0) {
		//	//	printf_s("ERR: seed cannot be negative\n"); return 1;
		//	//}
		//	NUM_GAMES = 1;
		//}

		printf_s("Set printout level, 0/1/2: [%i]  ", myruninfo.SCREEN);
		std::getline(std::cin, bufstr);
		if (bufstr.size() != 0) {
			// convert and apply
			myruninfo.SCREEN = atoi(bufstr.c_str());
			// NOTE: if the input is not numeric, it simply returns 0 instead of complaining
			if (myruninfo.SCREEN > 2) { myruninfo.SCREEN = 2; }
			if (myruninfo.SCREEN < 0) { myruninfo.SCREEN = 0; }
		}
		myruninfo.set_gamedata(tempsizex, tempsizey, tempnummines);
		return 0;
	} // end of LABEL_PROMPT and if there are no args

	for (int i = 1; i < argc; i++) {

		if (!strncmp(argv[i], "-h", 2) || !strncmp(argv[i], "-?", 2)) {
			printf_s("%s", helptext);
			//system("pause"); pause outside, not here... if -h is called its definitely from command-line
			return 1; // abort execution

		} else if (!strncmp(argv[i], "-pro", 4)) {
			goto LABEL_PROMPT;

		} else if (!strncmp(argv[i], "-def", 4)) {
			printf_s("Running with default values!\n");
			myruninfo.set_gamedata(tempsizex, tempsizey, tempnummines);
			return 0;

		} else if (!strncmp(argv[i], "-num", 4)) {
			if (argv[i + 1] != NULL) {
				myruninfo.NUM_GAMES = atoi(argv[i + 1]);
				// NOTE: if the argument at i+1 is not numeric, it simply returns 0 instead of complaining
				if (myruninfo.SPECIFY_SEED != 0)
					myruninfo.NUM_GAMES = 1;
				i++; continue;
			} else {
				printf_s("ERR: arg '%s' must be followed by a value, or else omitted!\n", argv[i]); return 1;
			}
		} else if (!strncmp(argv[i], "-field", 6)) {
			if (argv[i + 1] != NULL) {
				// ensure there are exactly two hyphens in the string
				int f = 0; int ct = 0;
				while (argv[i + 1][f] != '\0') {
					if (argv[i + 1][f] == '-') { ct++; }
					f++;
				}
				if (ct != 2) {
					printf_s("ERR: gamestring must have format '#-#-#', '%s' is unacceptable\n", argv[i + 1]); return 1;
				}
				// read the values and convert them to ints, then store them
				char * context = NULL;
				char * chunk = strtok_s(argv[i + 1], "-",&context);
				tempsizex = atoi(chunk);
				chunk = strtok_s(NULL, "-",&context);
				tempsizey = atoi(chunk);
				chunk = strtok_s(NULL, "-",&context);
				tempnummines = atoi(chunk);

				// error checking
				if (tempsizex < 1) { printf_s("ERR: sizeX cannot be zero or negative\n"); return 1; }
				if (tempsizey < 1) { printf_s("ERR: sizeY cannot be zero or negative\n"); return 1; }
				if (tempnummines >(tempsizex * tempsizey)) { printf_s("ERR: more mines than squares in the field!\n"); return 1; }
				if (tempnummines < 1) { printf_s("ERR: #mines cannot be zero or negative\n"); return 1; }

				i++; continue;
			} else {
				printf_s("ERR: arg '%s' must be followed by a value, or else omitted!\n", argv[i]); return 1;
			}
		} else if (!strncmp(argv[i], "-findz", 6)) {
			if (argv[i + 1] != NULL) {
				FIND_EARLY_ZEROS_var = bool(atoi(argv[i + 1]));
				// NOTE: if the argument at i+1 is not numeric, it simply returns 0 instead of complaining
				i++; continue;
			} else {
				printf_s("ERR: arg '%s' must be followed by a value, or else omitted!\n", argv[i]); return 1;
			}
		} else if (!strncmp(argv[i], "-smart", 6)) {
			if (argv[i + 1] != NULL) {
				RANDOM_USE_SMART_var = bool(atoi(argv[i + 1]));
				// NOTE: if the argument at i+1 is not numeric, it simply returns 0 instead of complaining
				i++; continue;
			} else {
				printf_s("ERR: arg '%s' must be followed by a value, or else omitted!\n", argv[i]); return 1;
			}
		} else if (!strncmp(argv[i], "-seed", 5)) {
			if (argv[i + 1] != NULL) {
				myruninfo.SPECIFY_SEED = atoi(argv[i + 1]);
				// NOTE: if the argument at i+1 is not numeric, it simply returns 0 instead of complaining
				myruninfo.NUM_GAMES = 1;
				i++; continue;
			} else {
				printf_s("ERR: arg '%s' must be followed by a value, or else omitted!\n", argv[i]); return 1;
			}
		} else if (!strncmp(argv[i], "-scr", 4)) {
			if (argv[i + 1] != NULL) {
				myruninfo.SCREEN = atoi(argv[i + 1]);
				// NOTE: if the argument at i+1 is not numeric, it simply returns 0 instead of complaining
				if (myruninfo.SCREEN > 2) { myruninfo.SCREEN = 2; }
				if (myruninfo.SCREEN < 0) { myruninfo.SCREEN = 0; }
				i++; continue;
			} else {
				printf_s("ERR: arg '%s' must be followed by a value, or else omitted!\n", argv[i]); return 1;
			}
		} else {
			printf_s("ERR: unknown argument '%s', print help with -h\n", argv[i]);
			return 1;
		}
	}
	myruninfo.set_gamedata(tempsizex, tempsizey, tempnummines);
	return 0;
}


// play one game with the field and zerolist as they currently are
// this way it's easier to lose from anywhere, without breaking out of loops
// return 1 = win, return 0 = loss, return -1 = unexpected loss
inline int play_game() {
	int r; // holds return value of any 'reveal' calls
	int consecutiveguesses = 0; // how many consecutive guesses
	char buffer[8];

	 // reveal one cell (chosen at random or guaranteed to succeed)
	if (!FIND_EARLY_ZEROS_var) {
		// reveal a random cell (game loss is possible!)
		mygamestats.luck_value_mult *= (1. - (float(myruninfo.get_NUM_MINES()) / float(mygame.unklist.size())));
		mygamestats.luck_value_sum += (1. - (float(myruninfo.get_NUM_MINES()) / float(mygame.unklist.size())));
		r = mygame.reveal(rand_from_list(&mygame.unklist));
		if (r == -1) { return 0; } // no need to log it, first-move loss when random-hunting is a handled situation
		// if going to use smartguess, just pretend that the first guess was a smartguess
		if(RANDOM_USE_SMART_var) { mygamestats.trans_map = "^ "; } else { mygamestats.trans_map = "r "; }
		// accumulate into luck value
	} else {
		// reveal a cell from the zerolist... game loss probably not possible, but whatever
		r = mygame.reveal(rand_from_list(&mygame.zerolist));
		if (r == -1) {
			myprintfn(2, "ERR: Unexpected loss during initial zerolist reveal, must investigate!!\n");
			return -1;
		}
		mygamestats.trans_map = "z ";
	}
	// add first entry to transition map
	mygamestats.print_gamestats(myruninfo.SCREEN, &mygame, &myruninfo);

	

	// begin game-loop, continue looping until something returns
	while (1) {
		
		int action = 0; // flag indicating some action was taken this LOOP
		int numactions = 0; // how many actions have been taken during this STAGE

		// if # of mines remaining = # of unknown cells remaining, flag them and win!
		if (mygame.get_mines_remaining() == mygame.unklist.size()) {
			numactions = mygame.get_mines_remaining();
			while(!mygame.unklist.empty()) {
				r = mygame.set_flag(mygame.unklist.front());
				if (r == 1) {
					// validated win! time to return!
					// add entry to transition map, use 'numactions'
					sprintf_s(buffer, "s%i ", numactions);
					mygamestats.trans_map += buffer;
					return 1;
				}
			}
		}

		////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// begin single-cell logic loop
		while(1) {
			action = 0;
			for (int y = 0; y < myruninfo.get_SIZEY(); y++) { for (int x = 0; x < myruninfo.get_SIZEX(); x++) { // iterate over each cell
				class cell * me = &mygame.field[x][y];
				if (me->get_status() != VISIBLE) { continue; } // SKIP

				// don't need to calculate 'effective' because it is handled every time a flag is placed
				// therefore effective values are already correct
				r = strat_singlecell(me, &action);
				if (r == 1) {
					// if game is won, handle trans_map and return!
					sprintf_s(buffer, "s%i ", (numactions + action));
					mygamestats.trans_map += buffer;
					return 1;
				} else if (r == -1) { return -1; }// unexpected game loss, should be impossible!

			}}

			if (action != 0) {	// if something happened, then accumulate and don't break
				numactions += action;
				mygamestats.began_solving = true;
			} else {			// if nothing changed, then break the loop
				break;
			}
		} 
		////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// end single-cell logic loop

		//sprintf_s(buffer, "s%i ", numactions);
		//mygamestats.trans_map += buffer;
		//mygamestats.print_gamestats(SCREEN); // post-multicell print
		if (numactions != 0) {
			consecutiveguesses = 0;
			sprintf_s(buffer, "s%i ", numactions); // add entry to transition map
			mygamestats.trans_map += buffer;
			mygamestats.print_gamestats(myruninfo.SCREEN, &mygame, &myruninfo); // post-singlecell print
		}

		////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// begin multi-cell logic (loops 6 times at most before returning to singlecell, configurable)
		numactions = 0;
		//std::list<class cell *> clearlist; std::list<class cell *> flaglist;
		int numloops = 0;
		while (1) {
			action = 0; 
			//clearlist.clear(); flaglist.clear();
			for (int y = 0; y < myruninfo.get_SIZEY(); y++) { for (int x = 0; x < myruninfo.get_SIZEX(); x++) {// iterate over each cell
				class cell * me = &mygame.field[x][y];
				if ((me->get_status() != VISIBLE) || (me->get_effective() == 0)) { continue; } // SKIP
				
				// strategy 3: 121-cross
				r = strat_121_cross(me, &mygamestats, &action);
				if (r == -1) { return -1; } // unexpected game loss, should be impossible!

				// strategy 4: nonoverlap-flag
				r = strat_nonoverlap_flag(me, &mygamestats, &action);
				if (r == 1) {// game won!
					sprintf_s(buffer, "m%i ", (numactions + action));
					mygamestats.trans_map += buffer;
					return 1;
				}

				// strategy 5: nonoverlap-safe
				r = strat_nonoverlap_safe(me, &mygamestats, &action);
				if (r == -1) { return -1; } // unexpected game loss, should be impossible!
				
				
				/* // QUEUEING 
				r = strat_121_cross_Q(me, &mygamestats, &clearlist);
				r = strat_nonoverlap_flag_Q(me, &mygamestats, &flaglist);
				r = strat_nonoverlap_safe_Q(me, &mygamestats, &clearlist);
				*/
			}}
			
			/* // QUEUEING
			for (std::list<class cell *>::iterator cit = clearlist.begin(); cit != clearlist.end(); cit++) {
				r = mygame.reveal(*cit);
				if (r == -1) { return -1; } // unexpected game loss, should be impossible!
				action += r;
			}
			for (std::list<class cell *>::iterator fit = flaglist.begin(); fit != flaglist.end(); fit++) {
				r = mygame.set_flag(*fit);
				if (r == 1) {// game won!
					sprintf_s(buffer, "m%i ", (numactions + action));
					mygamestats.trans_map += buffer;
					return 1;
				}
				action++;
			}*/
			
			if (action != 0) {
				numactions += action;
				mygamestats.began_solving = true;
				numloops++;
				if (numloops >= MULTICELL_LOOP_CUTOFF) { break; }
			} else {
				break;
			}

		}
		////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// end multi-cell logic loop

		//sprintf_s(buffer, "m%i ", numactions);
		//mygamestats.trans_map += buffer;
		//mygamestats.print_gamestats(SCREEN); // post-multicell print
		if (numactions != 0) {
			// if something changed, don't do guessing, instead go back to singlecell
			consecutiveguesses = 0;
			sprintf_s(buffer, "m%i ", numactions);// add entry to transition map, use 'numactions'
			mygamestats.trans_map += buffer;
			mygamestats.print_gamestats(myruninfo.SCREEN, &mygame, &myruninfo); // post-multicell print

		} else {
			// nothing changed during multi-cell, so reveal a new cell

			////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			// begin GUESSING phase

			int winorlose = 10; // 10=continue, -1=unexpected loss, 0=normal loss, 1=win
			char trans_map_char = '0';
			int trans_map_val = 0;
			bool isaguess = false;

			/* new guessing phase structure:
			if zeroguess
				do it
				isaguess=true, set trans_map_char
			else if random guess
				do it
				isaguess=true, set trans_map_char
			else if smart guess
				chain build/optimize
				if something was flagged/cleared,
					trans_map_val = action#, set trans_map_char, consecutiveguesses=0
				else if nothing was flagged/cleared,
					smartguess
					if just a guess,
						isaguess=true, set trans_map_char
					else if chain answer mode,
						trans_map_val = action#, set trans_map_char, consecutiveguesses=0
			if isaguess
				inc guesses, set trans_map_val = guesses
				trans_map erase prev
			trans_map add entry with trans_map_char and trans_map_val
			process delayed win-or-lose
			*/



			// actually guess, one of 5 endpoints...
			// To win when guessing, if every guess is successful, it will reveal information that the SC/MC
			// logic will use to place the final flags. So, the only way to win is by revealing the right safe places.
			if ((FIND_EARLY_ZEROS_var) && (mygame.zerolist.size()) && (mygamestats.began_solving == false)) {
				// option A: reveal one cell from the zerolist... game loss probably not possible, but whatever
				r = mygame.reveal(rand_from_list(&mygame.zerolist));
				if (r == -1) {
					myprintfn(2, "ERR: Unexpected loss during hunting zerolist reveal, must investigate!!\n");
					winorlose = -1;
				}
				trans_map_char = 'z'; isaguess = true;
			} else if(!RANDOM_USE_SMART_var) {
				// option B: random-guess
				mygamestats.luck_value_mult *= (1. - (float(mygame.get_mines_remaining()) / float(mygame.unklist.size())));
				mygamestats.luck_value_sum += (1. - (float(mygame.get_mines_remaining()) / float(mygame.unklist.size())));
				r = mygame.reveal(rand_from_list(&mygame.unklist));
				if (r == -1) {
					winorlose = 0; // normal loss
				}
				trans_map_char = 'r'; isaguess = true;
			} else {
				// option C: smart-guess
				// note: with random-guessing, old smartguess, it will only return 1 cell to clear
				// with NEW smartguess, it will usually return 1 cell to clear, but it may return several to clear or several to flag

				struct chain fullchain = chain();
				r = strat_chain_builder_optimizer(&fullchain, &trans_map_val);
				if (r != 0) { return r; }	// if win, return; if lose, return as unexpeced loss
				if (trans_map_val != 0) {	// if something was flagged/cleared, 
					consecutiveguesses = 0; trans_map_char = 'O';
				} else {					// if nothing was flagged/cleared, continue with smartguess
					struct smartguess_return rx = smartguess(&fullchain, &mygamestats);

					// TODO: make it clear the cells internally!!! eliminate the 'return the cells' convention!!
					for (int i = 0; i < rx.flagme.size(); i++) { // flag-list (uncommon but possible)
						r = mygame.set_flag(rx.flagme[i]);
						if (r == 1) {
							// fall thru, re-use the stat tracking code and return there
							winorlose = 1; break;
						}
					}
					for (int i = 0; i < rx.clearme.size(); i++) { // clear-list
						r = mygame.reveal(rx.clearme[i]);
						if (r == -1) {
							winorlose = 0; break;
						}
					}
					
					if (rx.method == 1) { // if it was a guess,
						isaguess = true; trans_map_char = '^';
					} else if (rx.method == 2) { // if it was the chain-solver mode,
						consecutiveguesses = 0; trans_map_char = 'A'; trans_map_val = rx.size();
					}
				}				
			}

			// handle stats and trans_map in a modular way, so this block handles any of the branches it may take above
			if (isaguess) {
				mygamestats.num_guesses++; consecutiveguesses++; trans_map_val = consecutiveguesses; // guessing
				if (consecutiveguesses >= 2) { // if this is the second+ hunt in a string of hunts, need to remove previous trans_map entry
					// method: (numhunts-1) to string, get length, delete that + 2 trailing chars from trans_map
					int m = (std::to_string(consecutiveguesses - 1)).size() + 2; // almost always going to be 3 or 4
					mygamestats.trans_map.erase(mygamestats.trans_map.size() - m, m);
				}
			}
			sprintf_s(buffer, "%c%i ", trans_map_char, trans_map_val);
			mygamestats.trans_map += buffer;

			// handle delayed winorlose return
			if (winorlose != 10) return winorlose;
			// post-guess print
			mygamestats.print_gamestats(myruninfo.SCREEN, &mygame, &myruninfo); 
		}

	}

	// should never hit this
	myprintfn(2, "ERR: Hit end of 'play_game' function without returning 1 or 0, must investigate!!\n");
	return -1;
}


// ************************************************************************************************
int main(int argc, char *argv[]) {
	// full-run init:
	// parse and store the input args here
	// note to self: argc has # of input args, INCLUDING PROGRAM NAME, argv is pointers to c-strings
	if (parse_input_args(argc, argv)==1) {
		if(argc==1) {
			system("pause"); // I know it was run from Windows Explorer (probably double-click) so pause before it closes
		}
		return 0; // abort execution
	}


	// open log file stream
	time_t t = time(0);
	struct tm now;
	localtime_s(&now, &t);
	std::string buffer(80, '\0');
	CreateDirectory("./LOGS/",NULL);
	// TODO: should put brief error catching for CreateDirectory line, but I don't care
	strftime(&buffer[0], buffer.size(), "./LOGS/minesweeper_%m%d%H%M%S.log", &now);
	fopen_s(&myruninfo.logfile, buffer.c_str(), "w");
	if (!myruninfo.logfile) {
		printf_s("File opening failed, '%s'\n", buffer.c_str());
		system("pause");
		return 1;
	}
	myprintfn(2, "Logfile success! Created '%s'\n\n", buffer.c_str());
	myprintfn(2, "Beginning MinesweeperSolver version %s\n", VERSION_STRING_def);

	// logfile header info: mostly everything from the #defines
	myprintfn(2, "Going to play %i games, with X/Y/mines = %i/%i/%i\n", myruninfo.NUM_GAMES, myruninfo.get_SIZEX(), myruninfo.get_SIZEY(), myruninfo.get_NUM_MINES());
	if (FIND_EARLY_ZEROS_var) {
		myprintfn(2, "Using 'hunting' method = succeed early (uncover only zeroes until solving can begin)\n");
	} else {
		myprintfn(2, "Using 'hunting' method = human-like (can lose at any stage)\n");
	}

	if (RANDOM_USE_SMART_var) {
		myprintfn(2, "Using 'guessing' mode = smartguess (slower but increased winrate)\n");
	} else {
		myprintfn(2, "Using 'guessing' mode = guess randomly (lower winrate but faster)\n");
	}

	// construct a trivial random generator engine from a time-based seed:
	unsigned int seed = std::chrono::duration_cast< std::chrono::milliseconds >( 
		std::chrono::system_clock::now().time_since_epoch() 
		).count();
	std::default_random_engine generator(seed);
	std::uniform_int_distribution<int> distribution(1, INT_MAX-1);
	// note, to invoke use: distribution(generator);


	// seed random # generator
	if (myruninfo.SPECIFY_SEED == 0) {
		// create a new seed from time, log it, apply it
		myprintfn(2, "Generating new seeds\n");
	} else {
		srand(myruninfo.SPECIFY_SEED);
		myruninfo.NUM_GAMES = 1;
		myprintfn(2, "Using seed %i for game 1\n", myruninfo.SPECIFY_SEED);
	}
	fflush(myruninfo.logfile);


	// init the 'game' object with the proper size
	mygame.init(myruninfo.get_SIZEX(), myruninfo.get_SIZEY());
	myrunstats.init_histogram(myruninfo.get_NUM_MINES());


	for (int game = 0; game < myruninfo.NUM_GAMES; game++) {

		// generate a new seed, log it, apply it... BUT only if not on the first game
		if (myruninfo.SPECIFY_SEED == 0) {
			int f = distribution(generator);
			myprintfn(myruninfo.SCREEN + 1, "Using seed %i for game %i\n", f, game + 1);
			srand(f);
		}
		// status tracker for impatient people
		printf_s("Beginning game %i of %i\n", (game + 1), myruninfo.NUM_GAMES);


		// reset stat-tracking variables
		mygamestats = game_stats();

		// reset everything and generate new field, count how many 8-cells in the new field
		myrunstats.games_with_eights += mygame.reset_for_game();

		// play a game, capture the return value
		int r = play_game();


		myrunstats.strat_121_total +=		mygamestats.strat_121;
		myrunstats.strat_nov_flag_total +=	mygamestats.strat_nov_flag;
		myrunstats.strat_nov_safe_total +=	mygamestats.strat_nov_safe;
		myrunstats.smartguess_attempts_total +=	mygamestats.smartguess_attempts;
		myrunstats.smartguess_diff_total +=		mygamestats.smartguess_diff;
		myrunstats.smartguess_valves_tripped_total += mygamestats.smartguess_valves_tripped;
		myrunstats.total_luck_per_guess += mygamestats.luck_value_sum;

		// increment run results depending on gamestate and gameresult
		myrunstats.games_total++;
		if (r == 0) { // game loss
			mygamestats.trans_map += "X";
			myrunstats.games_lost++;
			myrunstats.inc_histogram(myruninfo.get_NUM_MINES() - mygame.get_mines_remaining());
			// ................................. unused
			myrunstats.num_guesses_in_losses += mygamestats.num_guesses;
			myrunstats.total_luck_in_losses += mygamestats.luck_value_mult;
			if (mygamestats.began_solving == false) {
				myrunstats.games_lost_beginning++;
			}
			float remaining = float(mygame.get_mines_remaining()) / float(myruninfo.get_NUM_MINES());
			if (remaining > 0.85) {
				myrunstats.games_lost_earlygame++; // 0-15% completed
			} else if (remaining > 0.15) {
				myrunstats.games_lost_midgame++; // 15-85% completed
			} else {
				myrunstats.games_lost_lategame++; // 85-100% completed
			}
			// .................................
		} else if (r == 1) { // game win
			mygamestats.trans_map += "W";
			myrunstats.games_won++;
			myrunstats.num_guesses_in_wins += mygamestats.num_guesses;
			myrunstats.total_luck_in_wins += mygamestats.luck_value_mult;
			if (mygamestats.num_guesses > 0) {
				myrunstats.games_won_guessing++;
			} else {
				myrunstats.games_won_noguessing++;
			}
		} else if (r == -1) {
			myrunstats.games_lost_unexpectedly++;
		}

		// print/log single-game results (also to console if #debug)
		mygamestats.print_gamestats(myruninfo.SCREEN + 1, &mygame, &myruninfo);

		//printf_s("Finished game %i of %i\n", (game+1), NUM_GAMES);
	}

	// done with games!
	myrunstats.print_final_stats(&myruninfo);

	fclose(myruninfo.logfile);
	system("pause");
    return 0;
}

