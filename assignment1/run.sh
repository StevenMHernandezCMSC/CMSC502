make clean

make

sh gen_cities.sh 12 > tmp.txt

time ./main tmp.txt

# TODO: compare times and such on each type.