#!/bin/bash

# Author: Bc. Juraj Mariani, xmaria03 <xmaria03@stud.fit.vutbr.cz>

# Program can run with any number of processes greater that one
# Star core process and one surface process is the bare minimum

# The project was constructed in a way to support virtually infinite game area (limit to UINT16_MAX)
# The lack of finality has been poorly testet though
# The behaviour past 450-500 rounds is undefined (reason is unknown)

# I apologize for the zipped format, I had no idea there was a 2 file limit

if [ $# -lt 2 ]; then
    echo "No File and/or Iteration Count given." >&2
    exit 1
else
    file=$1
    rnds=$2;
    if [ ! -f $file ]; then
        echo "File specified (\"$file\") does not exist." >&2
        exit 1
    fi
fi;
proc=7
#preklad zdrojoveho souboru
g++ game_state.cpp -c
mpic++ life.cpp -c
mpic++ --prefix /usr/local/share/OpenMPI -o life life.o game_state.o

#spusteni programu
mpirun --prefix /usr/local/share/OpenMPI --use-hwthread-cpus -np $proc life $rnds < $file #2> stderr.txt 

#vypis return codu
#echo "rc: $?"

#uklid
rm -f *.o
rm -f life


