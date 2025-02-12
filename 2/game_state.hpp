/**
 * @file game_state.hpp
 * @author Bc. Juraj Mariani, xmaria03 <xmaria03@stud.fit.vutbr.cz>
 * @brief Game of Life Game State class
 * @version 1.0
 * @date 2024-04-18
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef __game_state__
#define __game_state__

#include <cstdio>
#include <cstdint>

// #define BITS (sizeof(uint64_t) * 8)
#define BITS 64

/**
 * @brief Structure for the (almost) infinite line space
 * LineArr -    array of 64bit integers, where each bit denotes a cell
 * nelems  -    number of cells (bits) stored within the array
 * nblocks -    number of blocks allocated for lineArr
 */
struct line {
    uint64_t* lineArr;
    uint16_t nelems;
    uint8_t nblocks;
};

inline void registerAlive(uint64_t* line, const uint8_t pos)
{
    *line = (*line) | (((uint64_t)1) << ((64 - pos) - 1));
}

inline void registerDead(uint64_t* line, const uint8_t pos)
{
    *line = (*line) & (~(((uint64_t)1) << ((64 - pos) - 1)));
}

inline void registerState(uint64_t* line, const uint8_t pos, bool alive)
{
    alive ? registerAlive(line, pos) : registerDead(line, pos);
}

inline bool isAlive(const uint64_t line, const uint8_t pos)
{
    return (line & (((uint64_t)1) << ((64 - pos) - 1)));
}

/**
 * @brief Enumeration for bounding rectangle values
 */
enum rect { xmin, xmax, ymin, ymax };

/**
 * @brief Class allowing storage and modification to the game of life (GOL) game state
 */
class GameState {
    public:
        GameState();
        bool initGameState();
        ~GameState();
        bool loadGameState(FILE* stream);
        bool prePorcessForIteration();
        line* getRow(uint16_t idx);
        uint16_t getNRows();
        void printGameState(uint8_t* procArr = nullptr);
        bool rowIsEmpty(uint16_t idx);
        bool addOuterRing();
        bool getBoundingRect(uint16_t buff[4]);

    private:
        line* rows;
        uint16_t nrows;

        int8_t getValFromStream(FILE* __stream);
        bool addLineToRow(uint16_t rowIdx);
        bool insertLineToRow(uint16_t rowIdx, uint16_t lineIdx);
        bool rmLineFromRow(uint16_t rowIdx);
        bool resizeLine(uint16_t rowIdx, uint16_t nElems);
        bool resizeLineByNBlocks(uint16_t rowIdx, uint8_t newNBlocks);
        bool resize(uint16_t nElems);
        bool addRow();
        bool insertRow(uint16_t rowIdx);
        bool rmRow(uint16_t idx);
        bool rmMultiRow(uint16_t fromIdx, uint16_t toIdx, bool including = true);
        bool padLines();
        bool shiftLineHor(uint16_t row, uint8_t offset, bool left);
        bool shiftLinesHor(uint8_t offset, bool left);
        bool shiftLineVert(uint16_t row, uint8_t offset, bool up);
        bool getRowBounds(uint16_t rowIdx, uint16_t buff[2]);
        bool registerBorder(uint16_t rowIdx, bool front);
};

#endif