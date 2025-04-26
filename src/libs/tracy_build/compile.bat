clang -c TracyClient.cpp -o tracy.o -DTRACY_ENABLE
llvm-lib tracy.o
del tracy.o
