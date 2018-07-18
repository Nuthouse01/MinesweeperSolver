Brian J Henson
5/7/2018 MinesweeperProject.cpp
v4.7

README contents:
	Help text and arguments
	Normal stats (speed, winrate, etc)
	"Transition map" key
	Fundamental rules of Minesweeper
	Explanation of solver logic (toplevel architecture)
	Explanation of solver logic (The Five Rules)
	Explanation of solver logic (Smartguess algorithm)


	
/////////////////////////////////////////////////////////////////////////
Help text and arguments
This program is intended to generate and play a large number of Minesweeper
games to collect win/loss info or whatever other data I feel like. It applies
single-cell and multi-cell logical strategies as much as possible before
revealing any unknown cells, of course. An extensive log is generated showing
most of the stages of the solver algorithm. Each game is replayable by using the
corresponding seed in the log, for closer analysis or debugging.
*Usage/args:
   -h, -?:               Print this text, then exit
   -pro, -prompt:        Interactively enter various run/game settings.
       Automatically chosen when no args are given (like when double-clicked 
	   from Windows Explorer)
   -def, -default:       Run the program using default values
*To apply settings from the command-line, use any number of these:
   -num, -numgames:      How many games to play with these settings
   -field:               Field size and number of mines, format= #x-#y-#mines
   -findz, -findzero:    1=on, 0=off. If on, reveal zeroes until any logic
       can be applied. Not human-like but usually reaches end-game.
   -smart, -smartguess:  1=on, 0=off. Replaces random guessing method with
       speculative allocation of mines and risk calculation. Increases runtime
       by 2x(avg) but increases winrate.
   -seed:                0=random seed, other=specify seed. Suppresses -num 
       argument and plays only 1 game.
   -scr, -screen:        How much printed to screen. 0=minimal clutter,
       1=results for each game, 2=everything



/////////////////////////////////////////////////////////////////////////
Normal stats over a large number of games (100k):

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
"Transition map" key

The transition map is a string built as the algorithm is running. It indicates
the order in which the solver stages were used, and how much was done in each stage.

The game always begins with at least 1 guess (either r# or z# or ^#), this IS included in the string,
but not in other places that count guesses.

s# and m# represent the *total number of cells* cleared/flagged in those stages... all other keys
represent the *number of actions* taken, if it clears a zero and reveals more than 1 cell, 
its NOT reflected there.

X		game lost after preceeding action(X is shown on the field printout where it uncovered a mine)
W		game won after preceeding action
s#		number of cells flagged/revealed in single-cell logic
m#		number of cells flagged/revealed in multi-cell logic
r#		number of consecutive random guesses made
z#		number of consecutive "cheaty" zero-guesses made, only until solver can solve something
^#		number of consecutive smart-guesses made
O#		number of cells smartguess algorithm found to be guaranteed clear/flag during chain optimization
		(this uses an improved version of multi-cell logic)
A#		number of cells smartguess alg found to be a perfect solution to one of the chains (explained
		somewhere below, its kinda complicated)



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
2) multi-cell or overlap logic
3) guessing

My solver has very simple top-level architecture: obviously, an initial blind guess must be 
made to start the game. From there it proceeds as follows:
First, apply single-cell logic rules until exhausted. It's possible that no logic can be 
  applied at all, but the solver proceeds the same whether or not it does.
Then, apply multi-cell logic rules until exhausted. If ANY multi-cell logic is applied, 
  loop back and try single-cell again; if NO multi-cell logic is applied, then fall through.
Lastly, guess an uncertain cell to reveal. If using smart-guess, it may instead determine 
  that some cells are definitely-safe or definitely-flag. In either case, do this once, then
  loop back and try single-cell again.

The solver differs from the human-playable version in that the game is won when the last
mine is flagged; it doesn't require all the non-mine cells to be revealed. The solver also
has read-access to the hidden information for the purposes of stat-tracking & error-checking
(the algorithm only places flags on cells that are DEFINITELY mines, so it verifies that
each cell flagged really is a mine, and errors if it isn't), and write-only access to the
"effective" value for efficiency's sake. This isn't enforced by permissions and public/private
access functions, but just trust me, it doesn't cheat.



/////////////////////////////////////////////////////////////////////////
Explanation of solver logic (The Five Rules)

The solver uses 5 logical rules to determine whether cells are mines or not. 2 are in single-
cell logic, and 3 are in multi-cell logic. First, I have to explain the idea of "effective value":
a cell's effective value equals its adjacency # minus how many flags are already adjacent to it.
This is a very useful shorthand.

In the examples, a number means the adjacency # or effective value, - means an unknown cell, ~ means
unknown cell that is definitely safe/flag (depending on the rule being demonstrated).

The rules are as follows:


1) single-cell: if an X-adjacency cell is next to X flags, all remaining adj unknowns are safe to reveal
alternate: if a cell has effective value == 0, all remaining adj unknowns are safe to reveal

You shouldn't need a graphic to understand this rule... if there are 0 mines in 3 cells, then all 3 are safe.


2) single-cell: if an X-effective cell is next to X unknowns, flag them all

You shouldn't need a graphic to understand this rule... if there are 3 mines in 3 cells, then all 3 are mines.


3) multi-cell: nonoverlap-flag
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


4) multi-cell: nonoverlap-safe
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


5) multi-cell: 121-cross
If three cells are in a horiz or vert line, with the effective values 1 then 2 then 1, I know the two
cells that would make it form a + are safe. This isn't logic so much as a pattern I noticed while
playing... I suppose that, out of all possible ways to place mines around a 121 line, any allocation that
places mines touching the 2 will necessarily violate their adjacency numbers. Just think about it for
a while, try to prove me wrong.

Example:
+++++++++++++
+ - - ~ - - +
+ - 1 2 1 - +
+ - - ~ - - +
+++++++++++++



/////////////////////////////////////////////////////////////////////////
Explanation of solver logic (Smartguess algorithm)

To really understand the smartguess and the recursive algorithm, just look at the source code,
I don't wanna write it out here. This is just an overveiw.

I'm really quite proud of the smartguess algorithm, it took a looooooong time to get right. The
alg determines the risk of all "border cells" (cells which are next to a visible adjacency #, and
I therefore have some infomation about) compared to the risk of all "interior cells" (cells I have
no information about), and chooses one with the lowest chance of being a mine. The risk for a
border cell is found by averaging the risk % from all pods the cell is present in. The interior 
risk is found by using a recursive algorithm to find all possible ways to place mines in the border 
cells, finding the average # of mines that would be placed in the border cells, and subtracting that
from the total # of mines remaining to know the # of mines in the interior cells.

Some terminology: 
A "pod" is a visible adjacency # and its associated adjancent unknowns; if you find yourself saying 
"there are 3 mines in these 5 cells" etc, that designates a pod. The algorithm optimizes recursion 
by eliminating duplicate pods (same value, same set of unknowns) and subtracting them in the case
that one is a pure subset of another.
A "link" is an unknown cell shared by 2 or more pods, and contains a reference to each.
A "chain" is a sequence of overlapping/linked pods; each chain can be solved independently.

The optimization stage may conclude that some of the cells are definitely mines or definitely safe,
before even doing the recursion. If so, just return with those and see if single-cell or multi-cell
can take over from there.

When below a certain threshold of mines remaining, the recursive alg also returns the actual 
cells chosen to produce each unique answer. If the sum of the minimum answers for each chain exactly
equals the number of mines remaining, if each minimum answer can be formed by only one allocation,
then apply that arrangement of flags... we also know that any interior cells are definitely safe.
Similarly, if the sum of the maximum answers for each chain, plus the number of interior cells,
is exactly equal to the number of mines remaining, then we know that all interior cells are definitely
mines. Also, if each maximum answer can be formed only by one specific allocation, then that answer
is also definitely the solution. Any situation other than these two contains some ambiguity and a
risk-based guess must still be made.

Examples:
this might be solved by this1 or         this2
+++++++++          +++++++++          +++++++++
+   1   +          +       +          +       +
+ - -   +          +   *   +          + *     +
+ - - 1 +          + ?     +          + ? *   +
+++++++++          +++++++++          +++++++++
if there is only 1 mine remaining, then I deduce that this1 with ?=safe is true
if there is 3 mines remaining, then I deduce that this2 with ?=mine is true
if there is 2 mines remaining, then it is fulfilled by either this1 with ?=mine, or this2 with ?=safe... I cannot know which



















