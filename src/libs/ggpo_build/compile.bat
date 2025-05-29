clang++ -D _WINDOWS -Wno-format -c lib.cpp -o ggpo.o -I./ -I../
llvm-lib ggpo.o
del ggpo.o
