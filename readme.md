# PRL 2023/2024

## Bc. Juraj Mariani, xmaria03, <xmaria03@stud.fit.vutbr.cz>

## Project No. 1 - Pipeline Merge Sort

We were to implement a [Pipeline Merge Sort](http://www.fnemec.cz/projects/mgr/sem2/xnemec61-prl-pms-doc.pdf) ([1](https://www.cs.colostate.edu/~cs475/f14/Lectures/pipelineSort.pdf)) using the Message Passing Interfece library OpenMPI.
10/10pts

## Project No. 2 - Game of Life (Using OpenMPI)

Implementation of *Conway's Game of Life*
7.6/10pts
Teacher's note:
```txt
"- ve výpisu vrací řádky a sloupce "
```

### Implementation details

I chose a fairly difficult implementation where the game state is recorded in a matrix of 64bit integers.
Each bit represents one cell. In theory, this implementation *should* support infinite game size. 
Although, artefacts have been observed in larger  sizes (exact numbers are unknown/forgotten).
Artefacts are a direct consequence of manual memory management (using malloc and free) and bitwise operations
to extract and record the state.

There may be an incorrect output format; based on the teacher's note.

