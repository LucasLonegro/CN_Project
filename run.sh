rm -rf ./reports
gcc -pedantic -Wall -std=c99 -g *.c -lm
./a.out
cd ./reports
latexmk -pdf *.tex
latexmk -c
