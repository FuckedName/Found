rm -rf main
clear
gcc -g -rdynamic -o main main.c  -std=c99 -lpthread 
./main
