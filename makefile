.RECIPEPREFIX = >

./build/buzzerd: ./build src/buzzerd.cpp src/daemon.cpp ./src/daemon.h ./src/client.cpp ./src/client.h ./src/ConfigHandler.cpp ./src/ConfigHandler.h
> g++ -Wall -O3 -o ./build/buzzerd ./src/buzzerd.cpp ./src/daemon.cpp ./src/client.cpp ./src/ConfigHandler.cpp -l bcm2835

./build:
> mkdir build

install: /usr/bin/buzzerd /etc/buzzerd.conf

/usr/bin/buzzerd: ./build/buzzerd
>cp ./build/buzzerd /usr/bin/buzzerd

/etc/buzzerd.conf: ./src/buzzerd.conf
>cp ./src/buzzerd.conf /etc/buzzerd.conf

doc: buzzerd.html

buzzerd.html: README.md
> pandoc README.md > ./buzzerd.html
