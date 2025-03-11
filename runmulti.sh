rm -rf ./reports
gcc -pedantic -Wall -std=c99 -g *.c -lm
./a.out
cd ./reports
count=0
N=12
for filename in ./*.tex; do
    latexmk -silent -pdf "${filename}" &
    (( ++count % N == 0)) && wait
done
latexmk -c
