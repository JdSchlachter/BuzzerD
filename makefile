.RECIPEPREFIX = >

./build/buzzerd : build src/buzzerd.cpp src/daemon.cpp ./src/daemon.h ./src/client.cpp ./src/client.h
> g++ -Wall -O3 -o ./build/buzzerd ./src/buzzerd.cpp ./src/daemon.cpp ./src/client.cpp -l bcm2835

./build :
>mkdir build

doc: buzzerd.html

buzzerd.html: README.md
>pandoc README.md > ./buzzerd.html  
