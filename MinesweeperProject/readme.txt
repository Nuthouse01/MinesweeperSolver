Brian J Henson
5/7/2018 MinesweeperProject.cpp
v4.7

README contents:
	Help text and arguments
	Normal stats (speed, winrate, etc)
	Explanation of some stats on the end-screen
	"Transition map" key
	Fundamental rules of Minesweeper
	Explanation of solver logic (toplevel architecture)
	Explanation of solver logic (The Five Rules)
	Explanation of solver logic (guessing)


	
/////////////////////////////////////////////////////////////////////////
Help text and arguments
This program is intended to generate and play a large number of Minesweeper
games to collect win/loss info or whatever other data I feel like. It applies
single-cell and multi-cell logical strategies as much as possible before
revealing any unknown cells, of course. An extensive log is generated showing
most of the stages of the solver algorithm. Each game is replayable by using the
corresponding seed in the log, for closer analysis or debugging.
*Usage/args:
   -h, -?:             Print this text, then exit.
   -pro, -prompt:      Interactively enter various run/game settings.
         Automatically chosen when no args are given (like when double-
		 clicked from Windows Explorer).
   -def, -default:     Run the program using default values.
*To apply settings from the command-line, use any number of these:
   -num, -numgames:    How many games to play with these settings.
   -field:             Field size and number of mines, format= #x-#y-#mines.
   -findz, -findzero:  1=on, 0=off. If on, reveal zeroes during earlygame.
         Not human-like but more reliably reaches end-game.
   -gmode:             Which guessing method to use. 0=random (fastest),
	     1=smartguess (slower but higher accuracy), 2=perfectmode (slower &\n\
         highest mem usage, but has highest accuracy).\n\
   -seed:              0=random seed, other=specify seed. Suppresses -num 
         argument and plays only 1 game.
   -scr, -screen:      How much printed to screen. 0=minimal clutter,
         1=results for each game, 2=everything



/////////////////////////////////////////////////////////////////////////
Normal stats over a large number of games (100k):
TODO: UPDATE

Winrate:
Human-guess, Random-guess:	24.6%
Human-guess, Smart-guess:	32.4%
Zero-guess,  Random-guess:	50.8%
Zero-guess,  Smart-guess:	59.9%
All winrates have variance of +/- 0.3%

Avg time per game:
Human-guess, Random-guess:	0.0009 sec
Human-guess, Smart-guess:	0.0021 sec
Zero-guess,  Random-guess:	0.0017 sec
Zero-guess,  Smart-guess:	0.0028 sec
Note that the speed drops drastically when the program is printing each stage to the screen/log



/////////////////////////////////////////////////////////////////////////
The results screen

Many of the entries on the post-run results screen are self-explanatory, but I'll try to describe
the more unusual ones:

The first entry on the results screen is a large bar graph (histogram) showing the distribution
  of the loses during the run. The question being answered here is "how far does the solver
  algorithm get through the game before it ends?" Wins are not included, and neither are losses
  that happen before the first flag is placed, since both categories normally dwarf the contents
  of the histogram. The top bar (extending horizontally) represents the number of games lost 
  very early on, and the bottom bar represents the number of games lost right before the end.
  The actual number of games in each category is shown on the left. The sum of all categories in
  the histogram should equal the "Games lost while solving (1-99%)" value below.
  
Unless using random-guessing, there will be an entry for "smartguess border est avg deviation" 
  (unless I deprecated that feature). When the smartguess function runs, part of the task is
  to estimate the number of mines among the border cells (explained more below). The displayed
  value is the average difference between this calculated value and the actual omniscient count
  of mines in the border; this can be an indicator of the accuracy of the smartguess' guesses.
  
Unless using random-guessing, there will also be "smartguess overflow count". The smartguess
  algorithm runs recursively, and will attempt to test EVERY SINGLE WAY that mines can be 
  arranged in the border cells. If there are more than 10,000 "leaves" in this recursive tree,
  the system will trigger a "safety valve" and sharply limit the number of branches that can
  happen at each level. With this feature, it will explore at most ~50k leaves in 0.5s (but this
  is very rare). Without this feature, it has been demonstrated to spend up to 5 minutes and 
  I don't even know how much memory simulating all ~4.3 BILLION arrangements. Which simply isn't 
  worth it for guessing one cell of one game. So, I implemented this overflow detector.

It also reports the "average safety per guess", which is simply the average chance that whenever
  guessing a cell, it is not a mine.

It displays the total number of games played, total wins vs total losses, and further breaks
  down each category. The right-most column will sum to 100% in wins and in losses. It shows
  how many wins didn't require any guesses (except the initial), vs how many needed to make 
  guesses to win. And it shows how may games were lost before a single flag was placed, vs 
  how many games were lost somewhere in the middle. 

There may be an additional category for "unexpected losses". (Unless there's a bug somewhere
  I haven't caught,) unexpected losses only occur immediately after a smartguess overflow,
  and even then they're quite rare. The logic assumes that the recursive algorithm tried every 
  single scenario, but when an overflow happens it skips many scenarios. The logic may
  produce incorrect results when given incomplete information, which results in a loss.



/////////////////////////////////////////////////////////////////////////
"Transition map" key

The transition map is a string built during each game as the algorithm is running. It indicates 
the order in which the solver stages were used within a game, and how much was done in each stage.
It is reset for each game.

The game always begins with at least 1 guess (either r or z or ^), this IS included in the string,
but not in other places that count guesses, since it is unavoidable. It does count towards the running
luck value, however.

s# and t# represent the *total number of cells* cleared/flagged in those stages... all other keys
represent the *number of actions* taken, if it clears a zero and reveals more than 1 cell, 
its NOT reflected there.

X		game lost after preceeding action(X is shown on the field printout where it uncovered a mine)
W		game won after preceeding action
s#		number of cells flagged/revealed in single-cell logic
t#		number of cells flagged/revealed in two-cell logic
z#		number of consecutive "cheaty" zero-guesses made, only until solver can solve something
r#		number of consecutive random guesses made
^#		number of consecutive smart-guesses made (whether perfectmode or normal)
M#		number of cells found definitely flag or definitely clear in multi-cell logic when optimizing 
				the chain before a smartguess (this uses an improved version of two-cell logic)
E#		number of cells that the endsolver (inside smartguess) found definitely flag or definitely clear



/////////////////////////////////////////////////////////////////////////
Fundamental rules of Minesweeper

In case you don't know how to play Minesweeper, this is how the game works.

A known number of "mines" are randomly placed in a field of known size. Every cell that doesn't
have a mine has an "adjacency number" which tells how many mines are in the 8 surrounding cells.
The cells' contents are then hidden from the player, and need to be "revealed" one at a time
(except that when a cell is revealed with 0 mines in the 8 surrounding cells, the game automatically
reveals those 8 surrounding cells). If they reveal a mine, they lose. The player can "flag" cells 
they believe to be mines, which locks it from being accidentally revealed, and the game is
won when all mines are flagged and all non-mine cells are revealed.

The information available to the player/solver is the status of each cell (hidden, flaged, or 
revealed); if revealed, they also have access to its adjacency number; the size of the field,
obviously; the total # of mines in the field; and how many flags they have placed.



/////////////////////////////////////////////////////////////////////////
Explanation of solver logic (toplevel architecture)

There are 3 "major stages" or "logic categories" for solving a minesweeper game: 
1) single-cell logic
2) two-cell or overlap logic
3) guessing

My solver has very simple top-level architecture: obviously, an initial blind guess must be 
made to start the game. From there it proceeds as follows:
First, apply single-cell logic rules until exhausted. It's possible that no logic can be 
  applied at all, but the solver proceeds the same whether or not it does.
Then, apply two-cell logic rules until exhausted. If ANY two-cell logic is applied, 
  loop back and try single-cell again; if NO two-cell logic is applied, then proceed to guessing.
Lastly, guess an uncertain cell to reveal. If using smart-guess, it may instead determine 
  that some cells are definitely-safe or definitely-flag. In either case, do this once, then
  loop back and try single-cell again.

The solver differs from the human-playable version in that the game is won when the last
mine is flagged; it doesn't require all the non-mine cells to be revealed. Security/permissions 
have been implemented, so that noone can cheat! All sensitive operations are handled by functions 
in MS_basegame.h/cpp, and that file should not be changed.
NOTE1: until further notice, 'cell.value' is exposed so the smartguess function can compare its
estimated # of mines in a chain against the actual # of mines in that chain, and know the
deviation.
NOTE2: even if 'cell.value' was re-privatized, I can't think of anyway to make the 'zeroguess'
feature work without enabling potential cheating. Starting out with a zero is inherently cheating!
To be totally cheat-proof, this must be removed entirely, or allow for only one zero-guess per game.
The current behavior allows repeated zero-guesses until at least 1 cell is flagged.



/////////////////////////////////////////////////////////////////////////
Explanation of solver logic (The Five Rules)

The solver uses 5 logical rules to determine whether cells are mines or not. 2 are in single-
cell logic, and 3 are in two-cell logic. First, I have to explain the idea of "effective value":
a cell's effective value equals its adjacency # minus how many flags are already adjacent to it.
This is a very useful shorthand.

In the examples, a number means the adjacency # or effective value, - means an unknown cell, ~ means
unknown cell that is definitely safe/flag (depending on the rule being demonstrated).

The rules are as follows:


1) single-cell: if an X-adjacency cell is next to X flags, all remaining adj unknowns are safe to reveal
alternate: if a cell has effective value == 0, all remaining adj unknowns are safe to reveal

You shouldn't need a graphic to understand this rule... if there are 0 mines among 3 cells, then all 3 are safe.


2) single-cell: if an X-effective cell is next to X unknowns, flag them all

You shouldn't need a graphic to understand this rule... if there are 3 mines among 3 cells, then all 3 are mines.


3) two-cell: nonoverlap-flag (flag the cell that isn't in the overlap area)
If two cells have effective values X and X+1, and all unknowns around X+1 (except for ONE) overlap with 
unknowns around X, that non-overlap cell must be a mine. Expanding to the general case, if two cells are 
have effective values X and X+Z, and X+Z has exactly Z nonoverlapping unknown cells, then all those cells 
must be mines.

Examples:
+++++++++
+ 1     +
+ - - ~ +
+   2   +
+++++++++
+++++++++++
+ - -     +
+ 2 - 5 ~ +
+ - - ~ ~ +
+++++++++++


4) two-cell: nonoverlap-safe (clear the cell that isn't in the overlap area)
If the adj unknowns of an X-effective cell are a pure subset of the adj unknowns of another cell with 
the same effective value, then the non-overlap section can be safely revealed!

Examples:
+++++++++
+ 1     +
+ - - ~ +
+   1   +
+++++++++
+++++++++++
+ ~ - -   +
+   3 3   +
+ ~ - -   +
+++++++++++


5) two-cell: 121-cross
If three cells are in a horiz or vert line, with the effective values 1 then 2 then 1, I know the two
cells that would make it form a + are safe. This isn't really logic, just based on examining all possible
way to place mines around a 121 line like this. Turns out, any allocation that places a mine touching the 
2 will necessarily violate their adjacency numbers. Just think about it for a while, try to prove me wrong.

Example:
+++++++++++++
+ - - ~ - - +
+ - 1 2 1 - +
+ - - ~ - - +
+++++++++++++



/////////////////////////////////////////////////////////////////////////
Explanation of solver logic (Smartguess algorithm)

To REALLY understand the smartguess and the recursive algorithm, just look at the source code,
I don't wanna write it out here. This is just an overview.

I'm really quite proud of the smartguess algorithm, it took a looooooong time to get right. The
alg determines the risk of all "border cells" (cells which are next to a visible adjacency #, and
I therefore have some infomation about) compared to the risk of all "interior cells" (cells I have
no information about), and chooses one with the lowest chance of being a mine. 

The interior risk is found by using a recursive algorithm to find all possible ways to place mines 
in the border cells, finding the average # of mines that would be placed in the border cells, and 
subtracting that from the total # of mines remaining to know the # of mines in the interior cells,
and since I know how many interior cells there are, I know their risk.

The risk for a border cell is calculated in different ways depending on whether using normal 
smartguess or "perfectmode", and whether below the "endsolver" threshold or not. In normal mode,
it is the average of the risk % of all pods it is a member in.

Some terminology: 
A "pod" is a visible adjacency # and any unknown cells adjacent to it; if you find yourself saying 
"there are 3 mines in these 5 cells" etc, that designates a pod.
A "link" is an unknown cell shared by 2 or more pods, aka an overlap cell.
A "chain" is a sequence of overlapping/linked pods; each chain can be solved independently.
A "solution" is a unique way of placing flags throughout the chain that completely satisfies all pods.
Each chain has many many many solutions.

The algorithm optimizes recursion by eliminating duplicate pods (same value, same set of unknowns)
and/or applying multicell logic to them to reduce their size or value. The optimization stage may 
conclude that some of the cells are definitely mines or definitely safe, before even doing the 
recursion. If so, just flag/reveal those and see if single-cell or two-cell can take over from there;
no need to make a risky guess when you have guaranteed-valid logic available!

The main difference between normal smartguess and "perfectmode" is that, in perfectmode, the recursive
portion collects "aggregate data" on the cells in the chain. If you think of it as a spreadsheet, 
each column represents one cell, and each row represents a unique way to allocate mines among the 
cells (one solution), like a string of 1s and 0s. At the very bottom, there's an additional row 
that contains the sums for each column(cell). That's the aggregate data, and lets me know that out of 
508 solutions found, this cell was flagged 12/508 times, or this cell was flagged 508/508 times, or
whatever. This gives me a superior method to determine the risk of the border cells, comared to the 
method used for normal guessing.

When below a certain threshold of mines remaining, the "endsolver" kicks in. This allows for more 
thorough exploration of the recursive "leaves" and more logic to be applied to the data returned.
In brief, it compares the solutions of the various chains against one another, and determines if
any solutions are guaranteed to be invalid or not.

Here is my comprehensive (but brief) notes copied straight from the code:

ROADMAP: the 11-step plan to total victory! ("11" does not include numbered sub-steps of podwise_recurse or endsolver)
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
			2: pick a pod, find all ways to saturate it, termed 'scenarios'. Then, for each scenario {
				3a: apply the changes according to the scenario by moving links from chosen pod to links_to_flag_or_clear
				3b: until links_to_flag_or_clear is empty, iterate! removing the links from any pods they appear in...
				3c: after each change, see if activepod has become 100/0/disjoint/negative... if so, add its links to links_to_flag_or_clear as flag or clear
				3d: after dealing with everything the link is connected to, deal with the actual link cell itself
				4: recurse! when it returns, combine the 'lower' info with 'thislvl' info (VERTICAL combining)
				5: combine the 'lower' info into 'retval', (HORIZONTAL combining)
			}
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




