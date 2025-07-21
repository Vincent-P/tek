:: clang++ -D _WINDOWS -Wno-format -c lib.cpp -o ggpo.o -I./ -I../
:: llvm-lib ggpo.o
:: del ggpo.o

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" x64
cl /c /EHsc /D_WINDOWS /external:W0 /external:I .. /external:I .  lib.cpp
lib lib.obj /out:ggpo.lib
del lib.obj 
