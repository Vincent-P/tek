clang -g -c TracyClient.cpp -o tracy.o -DTRACY_ENABLE
llvm-lib tracy.o
del tracy.o

:: call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" x64
:: cl /c /Z7 /EHsc /D_WINDOWS /DTRACY_ENABLE /external:W0 /external:I .. /external:I . /DEBUG:FULL  TracyClient.cpp
:: lib TracyClient.obj /out:tracy.lib
:: del TracyClient.obj
