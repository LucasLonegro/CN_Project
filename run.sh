rm -rf ./reports
gcc -pedantic -Wall -std=c99 -g *.c -lm
./a.out
cd ./reports
latexmk -silent -pdf *.tex
latexmk -c
