@ECHO OFF
ECHO beginning minesweeper tests in all 6 modes
PAUSE
::..\x64\Release\MinesweeperProject.exe -numgames 500000 -field 30-16-99 -findz 0 -gmode 0 -scr -1
::..\x64\Release\MinesweeperProject.exe -numgames 500000 -field 30-16-99 -findz 0 -gmode 1 -scr -1
::..\x64\Release\MinesweeperProject.exe -numgames 500000 -field 30-16-99 -findz 0 -gmode 2 -scr -1
::..\x64\Release\MinesweeperProject.exe -numgames 500000 -field 30-16-99 -findz 1 -gmode 0 -scr -1
::..\x64\Release\MinesweeperProject.exe -numgames 500000 -field 30-16-99 -findz 1 -gmode 1 -scr -1
::..\x64\Release\MinesweeperProject.exe -numgames 500000 -field 30-16-99 -findz 1 -gmode 2 -scr -1

..\x64\Release\MinesweeperProject.exe -numgames 500000 -field 16-16-40 -findz 0 -gmode 0 -scr -1
..\x64\Release\MinesweeperProject.exe -numgames 500000 -field 16-16-40 -findz 0 -gmode 1 -scr -1
..\x64\Release\MinesweeperProject.exe -numgames 500000 -field 16-16-40 -findz 0 -gmode 2 -scr -1
..\x64\Release\MinesweeperProject.exe -numgames 500000 -field 16-16-40 -findz 1 -gmode 0 -scr -1
..\x64\Release\MinesweeperProject.exe -numgames 500000 -field 16-16-40 -findz 1 -gmode 1 -scr -1
..\x64\Release\MinesweeperProject.exe -numgames 500000 -field 16-16-40 -findz 1 -gmode 2 -scr -1
ECHO EVERYTHING DONE!!
PAUSE
