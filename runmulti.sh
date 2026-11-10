#!/bin/bash
rm -rf ./reports
gcc -pedantic -Wall -std=c99 -g *.c -lm
./a.out "$@" || exit 1
cd ./reports
N=12
compile='count=0
for filename in ./*.tex; do
    latexmk -silent -pdf "${filename}" &
    (( ++count % '$N' == 0 )) && wait
done
wait
latexmk -c'
if command -v latexmk >/dev/null; then
    bash -c "$compile"
else
    docker run --rm -u "$(id -u):$(id -g)" -v "$PWD":/w -w /w texlive/texlive:latest bash -c "$compile"
fi
