@set nam=app
@set last=nothing
:loop
@for /f %%? in ('"forfiles.exe /P src /m main.c /c "cmd /c echo @ftime" "') do @set next=%%?
@if %next% == %last% goto wait
@set last=%next%
@echo %last%
call src\compile.bat
@if %errorlevel% neq 0 goto wait
game.exe
:wait
@timeout /t 1 /nobreak > nul
@goto loop
