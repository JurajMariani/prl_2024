/**
 * @file game_state.cpp
 * @author Bc. Juraj Mariani, xmaria03 <xmaria03@stud.fit.vutbr.cz>
 * @brief Game of Life Game State class
 * @version 1.0
 * @date 2024-04-18
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "game_state.hpp"
#include <cstdlib>
#include <cstdio>
#include <iostream>

/**
 * @brief Construct a new Game State:: Game State object
 */
GameState::GameState()
{
    this->rows = nullptr;
    this->nrows = 0;
}

/**
 * @brief Destroy the Game State:: Game State object
 * 
 */
GameState::~GameState()
{
    for (uint16_t i = 0; i <  this->nrows; i++) {
        if (this->rows[i].lineArr)
            free(this->rows[i].lineArr);
    }
    if (this->rows)
        free(this->rows);
    this->nrows = 0;
    this->rows = nullptr;
}

/**
 * @brief Initialize the Game State object
 * 
 * @return Success?
 */
bool GameState::initGameState()
{
    if (!this->addRow())
        return false;
    if (!this->resizeLineByNBlocks(0, 1))
        return false;
    this->rows[this->nrows - 1].lineArr[0] = 0;
    this->rows[this->nrows - 1].nelems = 0;
    return true;
}

/**
 * @brief Getter for a row specified by index idx
 * Returns a pointer so that the values inside can be changed
 * 
 * @param idx Row index
 * @return line* Row
 */
line* GameState::getRow(uint16_t idx)
{
    if (this->nrows <= idx) {
        // Errors were numbered for debugging purposes
        fprintf(stderr, "1. ERROR: Requested row is out of bounds.\n");
        return nullptr;
    }
    return &(this->rows[idx]);
}

/**
 * @brief Returns the number of rows within the Game State
 * 
 * @return uint16_t Number of rows
 */
uint16_t GameState::getNRows()
{
    return this->nrows;
}

/**
 * @brief Insert a new 64bit integer to the end of a specific row
 * 
 * @param rowIdx Row where a new line is created
 * @return Status
 */
bool GameState::addLineToRow(uint16_t rowIdx)
{
    if (rowIdx >= this->nrows) {
        fprintf(stderr, "2. ERROR: Row index out of bounds.\n");
        return false;
    }
    this->rows[rowIdx].nblocks++;
    if (!this->resizeLineByNBlocks(rowIdx, this->rows[rowIdx].nblocks)) {
        fprintf(stderr, "3. ERROR: MEMALLOC failed.\n");
        this->rows[rowIdx].nblocks--;
        return false;
    }
    this->rows[rowIdx].lineArr[this->rows[rowIdx].nblocks - 1] = 0;
    return true;
}

/**
 * @brief Insert a new 64bit integer to a chosen index of a specific row
 * 
 * @param rowIdx Row where a new line is created
 * @param lineIdx Line where the new empty line is created
 * @return Status
 */
bool GameState::insertLineToRow(uint16_t rowIdx, uint16_t lineIdx)
{
    if (rowIdx >= this->nrows) {
        fprintf(stderr, "4. ERROR: Row index out of bounds.\n");
        return false;
    }
    if (lineIdx > this->rows[rowIdx].nblocks) {
        fprintf(stderr, "5. ERROR: Row index out of bounds.\n");
        return false;
    }
    // A new line is created at the end
    if (!this->addLineToRow(rowIdx))
        return false;
    // If inserting between existing lines, element count needs to be increased
    if (lineIdx != (this->rows[rowIdx].nblocks - 1))
        this->rows[rowIdx].nelems += BITS;
    // Lines past the index (including) are shifted to the right
    // giving way for the new line
    for (uint16_t i = this->rows[rowIdx].nblocks; i > lineIdx; i--) {
        this->rows[rowIdx].lineArr[i] = this->rows[rowIdx].lineArr[i - 1];
    }
    return true;
}

/**
 * @brief Create an empty row at the bottom of the Game State
 * 
 * @return Status
 */
bool GameState::addRow()
{
    line* tmp = (line*) realloc(this->rows, (this->nrows + 1) * sizeof(line));
    if (!tmp) {
        fprintf(stderr, "6. ERROR: MEMALLOC failed.\n");
        return false;
    }
    this->rows = tmp;
    // Set default values for row
    this->rows[this->nrows].nelems = 0;
    this->rows[this->nrows].nblocks = 0;
    this->rows[this->nrows].lineArr = nullptr;
    this->nrows++;
    if (!this->addLineToRow(this->nrows - 1))
        return false;
    return true;
}

/**
 * @brief Create a new row in place of specified index
 * Rows past the index (including) are shifted down
 * 
 * @param rowIdx Row
 * @return Status 
 */
bool GameState::insertRow(uint16_t rowIdx)
{
    // Append an empty row
    if (!this->addRow())
        return false;
    uint64_t* tmp = this->rows[this->nrows - 1].lineArr;
    // Shift rows down to make way for new row
    for (uint16_t i = this->nrows - 2; (i >= rowIdx) && (i < nrows); i--) {
        if (!this->shiftLineVert(i, 1, false))
            return false;
    }
    // Store the new row to its new place
    this->rows[rowIdx].lineArr = tmp;
    this->rows[rowIdx].nelems = 0;
    this->rows[rowIdx].nblocks = 1;
    return true;
}

/**
 * @brief Remove row
 * 
 * @param idx Index of row
 * @return Status
 */
bool GameState::rmRow(uint16_t idx)
{
    if (idx >= this->nrows) {
        fprintf(stderr, "7. ERROR: Remove index out of bounds.\n");
        return false;
    }
    // If allocated, release assigned lineArr memory
    if (this->rows[idx].lineArr)
        free(this->rows[idx].lineArr);
    // following rows' line ptrs are elevated
    for (uint16_t i = idx + 1; i < this->nrows; i++) {
        this->rows[i - 1].lineArr = this->rows[i].lineArr;
        this->rows[i - 1].nelems = this->rows[i].nelems;
    }
    this->nrows--;
    line* tmp = (line*) realloc(this->rows, this->nrows * sizeof(line));
    if (!tmp) {
        fprintf(stderr, "8. ERROR: MEMALLOC failed\n");
        return false;
    }
    this->rows = tmp;
    return true;
}

/**
 * @brief Remove rows in interval
 * 
 * @param fromIdx start of the interval
 * @param toIdx end of the interval
 * @param including type of interval - () or <>
 * @return Status
 */
bool GameState::rmMultiRow(uint16_t fromIdx, uint16_t toIdx, bool including)
{
    // Swap indexes so that fromIndex is smaller
    if (fromIdx > toIdx) {
        uint16_t tmp = fromIdx;
        fromIdx = toIdx;
        toIdx = tmp;
    }
    // Edge case - do not remove anything
    if (((toIdx - fromIdx) == 1) && (!including))
        return true;
    uint8_t dec = (including ? 0 : 1);
    if (((fromIdx + dec) >= this->nrows) || ((toIdx - dec) >= this->nrows)) {
        fprintf(stderr, "9. ERROR: Input indices out of bounds.\n");
        return false;
    }
    uint16_t delta = (toIdx - dec) - (fromIdx + dec) + 1;
    if (delta > BITS) {
        fprintf(stderr, "10. ERROR: Exceeded the hard cap of %d.\n", BITS);
        return false;
    }
    // Call rmRow for each row of the interval
    for (uint16_t i = fromIdx + dec; i <= (toIdx - dec); i++)
        this->rmRow(fromIdx + dec);
    return true;
}

/**
 * @brief Test whether a row is empty
 * 
 * @param idx Row
 * @return Status
 */
bool GameState::rowIsEmpty(uint16_t idx)
{
    for (uint16_t i = 0; i < (this->rows[idx].nblocks); i++) {
        if (this->rows[idx].lineArr[i])
            return false;
    }

    return true;
}

/**
 * @brief Load Game State
 * 
 * @param stream FILE stream to read from
 * @return Status
 */
bool GameState::loadGameState(FILE* stream)
{
    if ((!this->rows) || (!this->rows[0].lineArr)) {
        fprintf(stderr, "11. ERROR: Uninitialized GameState.\n");
        return false;
    }

    int8_t ch;
    do {
        ch = this->getValFromStream(stream);
        if (ch < 0)
            break;
        if ((ch == 1) || (ch == 0))
            this->rows[this->nrows - 1].nelems++;

        if (!(this->rows[this->nrows - 1].nelems % BITS))
            if (!this->addLineToRow(this->nrows - 1))
                return false;
        if (ch == 10) {
            if (!this->addRow())
                return false;
        }
        if (ch == 1) {
            registerAlive(&(this->rows[this->nrows - 1].lineArr[this->rows[this->nrows - 1].nelems / BITS]), (this->rows[this->nrows - 1].nelems - 1) % BITS);
        }
    } while (ch >= 0);
    // Remove any empty rows in Game State
    for (uint16_t i = 0; i < this->nrows; i++) {
        if (this->rows[i].nelems == 0)
            this->rmRow(i);
    }
    return true;
}

/**
 * @brief Read value from stream and transform to 1 or 0
 * 
 * @param __stream Stream to read from
 * @return int8_t Integer value (not ASCII) of read character. Returns 1,0 for cells, 10 for newline and -1 for unsupported character
 */
int8_t GameState::getValFromStream(FILE* __stream)
{
    int8_t ch = fgetc(__stream);
    switch (ch) {
        case '0':
            return 0;
        case '1':
            return 1;
        case '\n':
            return 10;
        default:
            return -1;
    }
}

/**
 * @brief Prints Game State
 * Function is capable of displaying ProcRank
 * If nullptr is fiven displays only Game State
 * 
 * @param procArr Voluntary array with process ranks, needs to have an entry for each row
 */
void GameState::printGameState(uint8_t* procArr)
{
    for (uint16_t i = 0; i < this->nrows; i++) {
        if (procArr)
            printf("%d: ", procArr[i]);
        for (uint16_t j = 0; j < this->rows[i].nelems; j++) {
            printf("%c", isAlive(this->rows[i].lineArr[j / BITS], j % BITS) ? '1' : '0');
        }
        printf("\n");
    }
}

/**
 * @brief Shifts a row vertially
 * Function shifts only one line and does not free any resources
 * 
 * @param row Row to shift
 * @param offset By how much
 * @param up direction selector
 * @return Status
 */
bool GameState::shiftLineVert(uint16_t row, uint8_t offset, bool up)
{
    if (offset > BITS) {
        fprintf(stderr, "12. ERROR: Offset exceeded hard cap of %d cells.", BITS);
        return false;
    }
    if (row >= this->nrows) {
        fprintf(stderr, "13. ERROR: Row index out of bounds.\n");
        return false;
    }
    if (offset == 0)
        return true;

    uint16_t targetRow = 0;
    if (up)
        targetRow = row - offset;
    else
        targetRow = row + offset;

    if (targetRow > this->nrows) {
        fprintf(stderr, "14. ERROR: Target row index is out to bounds.\n");
        return false;
    }
    // It is assumed that target row -> lineArr is nullptr or otherwise handeled by caller
    this->rows[targetRow].lineArr = this->rows[row].lineArr;
    this->rows[targetRow].nblocks = this->rows[row].nblocks;
    this->rows[targetRow].nelems = this->rows[row].nelems;
    return true;
}

/**
 * @brief Shifts lines of a row horizontally
 * 
 * @param row Which row to shift
 * @param offset By how much (Cells/Bits)
 * @param left Direction of shift
 * @return Status
 */
bool GameState::shiftLineHor(uint16_t row, uint8_t offset, bool left)
{
    if (offset > BITS) {
        fprintf(stderr, "15. ERROR: Offset exceeded hard cap of %d cells.", BITS);
        return false;
    }
    if (row >= this->nrows) {
        fprintf(stderr, "16. ERROR: Row index out of bounds.\n");
        return false;
    }
    if (!this->rows[row].lineArr) {
        fprintf(stderr, "17. ERROR: Row initialized but no cells associated with it found.");    
        return false;
    }
    if (offset == 0)
        return true;

    uint64_t overflow = 0;
    uint64_t overflow_tmp = 0;
    uint8_t cellCnt = this->rows[row].nblocks;
    uint8_t* indexArray = (uint8_t*) malloc(cellCnt);
    if (!indexArray) {
        fprintf(stderr, "18. ERROR: MEMALLOC failed.");
        return false;
    }
    for (uint8_t i = 0; i < cellCnt; i++)
        indexArray[i] = (left ? cellCnt - i - 1 : i);

    for (uint8_t i = 0; i < cellCnt; i++) {
        if (left) {
            overflow_tmp = (this->rows[row].lineArr[indexArray[i]] >> (BITS - offset));
            this->rows[row].lineArr[indexArray[i]] = this->rows[row].lineArr[indexArray[i]] << offset;
        } else {
            overflow_tmp = (this->rows[row].lineArr[indexArray[i]] << (BITS - offset));
            this->rows[row].lineArr[indexArray[i]] = this->rows[row].lineArr[indexArray[i]] >> offset;
        }
        this->rows[row].lineArr[indexArray[i]] = this->rows[row].lineArr[indexArray[i]] | overflow;
        overflow = overflow_tmp;
    }
    this->rows[row].nelems -= (left) ? (offset) : (0 - offset);
    free(indexArray);
    // Nonzero overflow means something couldn't fit to the existing line
    // therefore a new line segment has to be added to the front/back 
    // depending on the value of 'left'
    if (overflow) {
        if (left) {
            // This scenario is ridiculous and counterproductive
        } else {
            this->rows[row].nelems += offset;
            if (!this->addLineToRow(row))
                return false;
            // Add overflow to new line
            this->rows[row].lineArr[this->rows[row].nblocks - 1] = overflow;
        }
    }
    // Adjust the nblocks by nelems
    // Necessary?
    //this->rows[row].nblocks = (this->rows[row].nelems / BITS) + 1;
    //this->resizeLineByNBlocks(row, this->rows[row].nblocks);
    return true;
}

/**
 * @brief Performs horizontal shift operation on all rows
 * 
 * @param offset No of bits/cells to shift
 * @param left Direction of shift
 * @return Status
 */
bool GameState::shiftLinesHor(uint8_t offset, bool left)
{
    for (uint16_t i = 0; i < this->nrows; i++) {
        if (!this->shiftLineHor(i, offset, left))
            return false;
    }

    return true;
}

/**
 * @brief Calculate points of bounding rectangle
 * Result coordinate order is denoted by enum rect
 * 
 * @param buff I/O initialized array of 4 16bit integers
 * @return Status
 */
bool GameState::getBoundingRect(uint16_t buff[4])
{
    bool fst = true;
    uint16_t row_bounds[2] = { 0 };
    for (uint16_t i = 0; i < this->nrows; i++) {
        if (!this->getRowBounds(i, row_bounds))
            continue;

        if (fst) {
            buff[xmin] = row_bounds[xmin];
            buff[xmax] = row_bounds[xmax];
            buff[ymin] = i;
            buff[ymax] = i;
            fst = false;
        } else {
            if (buff[xmax] < row_bounds[xmax])
                buff[xmax] = row_bounds[xmax];
            if (buff[xmin] > row_bounds[xmin])
                buff[xmin] = row_bounds[xmin];
            if (buff[ymax] < i)
                buff[ymax] = i;
        }
    }
    // If fst is true there is zero rows or all rows are empty
    return !fst;
}

/**
 * @brief Determines the position of first and last alive cells within a row
 * 
 * @param rowIdx Row
 * @param buff I/O initialized 2 16bit array
 * @return true = OK
 * @return false = all cells are dead
 */
bool GameState::getRowBounds(uint16_t rowIdx, uint16_t buff[2])
{
    bool fst = true;
    for (uint16_t i = 0; i < this->rows[rowIdx].nelems; i++) {
        if (isAlive(this->rows[rowIdx].lineArr[(i / BITS)], i)) {
            if (fst) {
                buff[0] = i;
                buff[1] = i;
                fst = false;    
            } else {
                buff[1] = i;
            }
        }
    }
    return !fst;
}

/**
 * @brief Add a border of dead cells surrounding current Game State
 * Exists because if applied on a bounding rect can within a span of one round
 * expand only by one cell in each direction
 * 
 * @return Status
 */
bool GameState::addOuterRing()
{
    // Insert empty row above
    if (!this->insertRow(0))
        return false;
    this->resizeLineByNBlocks(0, this->rows[1].nblocks);
    // Set the length of empty row to the length of other rows e.g. row 1 
    this->rows[0].nelems = this->rows[1].nelems;
    // Manually set values to zero
    for (uint16_t i = 0; i < (this->rows[1].nelems / BITS) + 1; i++)
        this->rows[0].lineArr[i] = 0;
    
    // Add row below
    if (!this->addRow())
        return false;
    this->resizeLineByNBlocks(this->nrows - 1, this->rows[0].nblocks);
    // Set length to the one of row 0
    this->rows[this->nrows - 1].nelems = this->rows[0].nelems;
    for (uint16_t i = 0; i < this->rows[this->nrows - 1].nblocks; i++)
        this->rows[this->nrows - 1].lineArr[i] = 0;
    // For each row add empty cell before and after
    for (uint16_t i = 0; i < this->nrows; i++) {
        if (!this->registerBorder(i, true))
            return false;
        if (!this->registerBorder(i, false))
            return false;
    }
    return true;
}

/**
 * @brief Pad a row with one dead cell
 * 
 * @param rowIdx Row
 * @param front Side selector
 * @return Status
 */
bool GameState::registerBorder(uint16_t rowIdx, bool front)
{
    uint16_t currDims = this->rows[rowIdx].nblocks;
    uint32_t nelems = this->rows[rowIdx].nelems;
    uint16_t nextDims = ((this->rows[rowIdx].nelems + 1) / BITS) + 1;
    
    if (currDims != nextDims) {
        if (!this->addLineToRow(rowIdx))
            return false;
        if (front) {
            if (!this->shiftLineHor(rowIdx, 1, false))
                return false;
        }
    } else {
        if (front) {
            if (!this->shiftLineHor(rowIdx, 1, false))
                return false;
        }
    }
    // Register added cell in element count
    this->rows[rowIdx].nelems = nelems + 1;
    return true;
}

/**
 * @brief Prprocess Game State
 * - Reduce Game State to BoundingRect
 * - Add padding of one cell
 * 
 * @return Status
 */
bool GameState::prePorcessForIteration()
{
    uint16_t dims[4];
    if (!this->getBoundingRect(dims))
        return false;
    // dims are ordered by enum rect
    // Operation:
    // - Shift all lines to the left by xmin
    if (!this->shiftLinesHor(dims[rect::xmin], true))
        return false;
    // - Shift or Remove all lines up by ymin
    for (uint16_t i = 0; i < dims[rect::ymin]; i++) {
        if (!this->rmRow(0))
            return false;
    }
    // - Resize all rows to (xmax - xmin + 1)
    if (!this->resize(dims[rect::xmax] - dims[rect::xmin] + 1))
        return false;
    // - Remove trailing empty rows
    if (!this->rmMultiRow(dims[rect::ymax] - dims[rect::ymin], this->nrows, false))
        return false;
    // Finalize by adding an outer ring
    if (!this->addOuterRing())
        return false;
    if (!this->getBoundingRect(dims))
        return false;
    return true;
}

/**
 * @brief Set and realloc all Rows to nElems elements
 * 
 * @param nElems No of elements
 * @return Status
 */
bool GameState::resize(uint16_t nElems)
{
    for (uint16_t i = 0; i < this->nrows; i++) {
        if (!this->resizeLine(i, nElems))
            return false;
    }

    return true;
}

/**
 * @brief Resize (and realloc) a row to have desired number of elements
 * 
 * @param rowIdx Row
 * @param nElems No of elements to be in row
 * @return Status
 */
bool GameState::resizeLine(uint16_t rowIdx, uint16_t nElems)
{
    if (rowIdx >= this->nrows) {
        fprintf(stderr, "ERROR: Row index out of bounds.\n");
        return false;
    }

    this->rows[rowIdx].nblocks = (nElems / BITS) + 1;
    uint64_t* tmp;
    
    this->rows[rowIdx].nelems = nElems;
    tmp = (uint64_t*) realloc(this->rows[rowIdx].lineArr, this->rows[rowIdx].nblocks);
    if (!tmp) {
        fprintf(stderr, "ERROR: MEMALLOC failed.\n");
        this->rows[rowIdx].nblocks = 0;
        return false;
    }
    this->rows[rowIdx].lineArr = tmp;

    return true;
}

/**
 * @brief Resize row but keep no of elements same
 * 
 * @param rowIdx Row
 * @param newNBlocks New number of blocks
 * @return Status 
 */
bool GameState::resizeLineByNBlocks(uint16_t rowIdx, uint8_t newNBlocks)
{
    if (rowIdx >= this->nrows) {
        fprintf(stderr, "ERROR: Row index out of bounds.\n");
        return false;
    }
    uint64_t* tmp = (uint64_t*) realloc(this->rows[rowIdx].lineArr, newNBlocks);
    if (!tmp) {
        fprintf(stderr, "ERROR: MEMALLOC failed.\n");
        return false;
    }
    this->rows[rowIdx].lineArr = tmp;
    this->rows[rowIdx].nblocks = newNBlocks;
    return true;
}