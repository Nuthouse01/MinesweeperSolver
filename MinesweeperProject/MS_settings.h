#ifndef MS_SETTINGS
#define MS_SETTINGS
// all the #define constants and configurable settings




/*
example distributions:
X/Y/mines
9/9/10   (MS easy)
16/16/40 (MS medium)
30/16/99 (MS expert)
30/16/90 (default used for all testing)
10/18/27 (phone easy)
10/18/38 (phone medium)
10/18/67 (phone IMPOSSIBLE)
*/

// #defines
//////////////////////////////////////////////////////////////////////////////////
// these #defines control default values, can be overridden by command-line args
#define NUM_GAMES_def				1000
#define SIZEX_def					30
#define SIZEY_def					16
#define NUM_MINES_def				90
// TODO: elminate find_early_zeros mode?
// will start the game by revealing a zero-cell, giving a huge advantage
#define	FIND_EARLY_ZEROS_def		false
// there are 3 guessing modes: 0=random, 1=smartguess, 2=perfectmode
#define GUESSING_MODE_def			2
#define VERSION_STRING_def			"v5.1"
// controls what gets printed to the console
// 0: prints almost nothing to screen, 1: prints game-end to screen, 2: prints everything to screen
// -1: logfile is empty except for run-end results
// 3: lots of extra smartguess debug info, everything to screen & log
#define SCREEN_def					1
// if SPECIFY_SEED = 0, will generate a new seed from current time
#define SPECIFY_SEED_def			0


//////////////////////////////////////////////////////////////////////////////////
// these #defines control other settings that should be fine-tuned by me, but not changed every run
#define HISTOGRAM_RESOLUTION		15
#define HISTOGRAM_MAX_HORIZ_SIZE	40

// after X loops, see if single-cell logic can take over... if not, will resume two-cell
// surprisingly two-cell logic seems to consume even more time than the recursive smartguess when this value is high
#define TWOCELL_LOOP_CUTOFF			3
// when there are fewer than X mines on the field, begin storing the actual cells flagged to create each answer found by recursion
// also attempt to solve a chain if there is exactly one perfect solution
#define SMARTGUESS_ENDSOLVER_THRESHOLD	14
// in recursive function, after finding X solutions, stop being so thorough... only test RECURSION_SAFE_WIDTH scenarios at each lvl
// this comes into play only very rarely even when set as high as 10k
// with the limiter at 10k, the highest # of solutions found is 51k, even then the algorithm takes < 1s
// without the limiter, very rare 'recursion rabbitholes' would find as many as 4.6million solutions to one chain, > 90sec
#define RECURSION_SAFETY_LIMITER	10000 
// when the 'safety valve' is tripped, try X scenarios at each level of recursion, no more
// probably should only be either 2 or 3
#define RECURSION_SAFE_WIDTH		2 


// in recursion, when taking the average of all solutions, weigh them by their relative likelihood or just do an unweighted avg
// this SHOULD decrease algorithm deviation and therefore increase winrate, but instead it somehow INCREASES average deviation and 
// winrate is unchanged... doesn't make sense at all >:(
#define USE_WEIGHTED_AVG			true
// change whether border risk is calculated via avg=0/max=1 of the contributing pods
#define RISK_CALC_METHOD			0


//// in recursive function, after recursing X layers down, check to see if the chain is fragmented (almost certainly is)
//// this should have no impact on the accuracy of the result, and testing has shown that over many games it has almost no impact on efficiency
//// NOTE: this must be >= SMARTGUESS_ENDSOLVER_THRESHOLD
//#define CHAIN_RECHECK_DEPTH			8


#endif
