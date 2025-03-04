gcc -pedantic -Wall -std=c99 *.c
./a.out
cd ./reports
latexmk -pdf *.tex
latexmk -c
