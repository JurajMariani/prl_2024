/**
 * @file life.cpp
 * @author Bc. Juraj Mariani, xmaria03 <xmaria03@stud.fit.vutbr.cz>
 * @brief Conway's Game of Life simulated implemented using a parallel achitecture star topology
 *
 * Communication protocol:
 * 
 *          Core (1)                            Surface (size - 1)
 *            |         START OF ROUND             |
 *        1a. |------- Start / Terminate --------->| 1b.
 *        2a. |<--------- AckReq / Ack ------------| 2b.
 *        3a. |----------- Ack / Deny ------------>| 3b.
 *        4a. |-------------- DATA --------------->| 4b.
 *            |                n x                 |
 *        4a. |-------------- DATA --------------->| 4b.
 *            |                                    |
 *            |     *Surface procs processing*     |
 *            |                                    |
 *        5a. |-------------- Pull --------------->| 5b.
 *        6a. |<----- Ack / Deny / Terminate ------| 6b.
 *        7a. |--------- Ack / Terminate --------->| 7b.
 *        8a. |<------------- DATA ----------------| 8b.
 *            |                n x                 |
 *        8a. |<------------- DATA ----------------| 8b.
 *        9a  |-------------- Ack ---------------->| 9b.
 *            |           END OF ROUND             |
 * 
 * Where to find Send/Recv:
 *   1a. and 2a. = startUp() or terminateAllSurfaceProcs()
 *   3a. = SimRound() 
 *   4a. = sendData()
 *   5. to 9a. = receiveData()
 * 
 *   1b. and 2b. = surfaceStartUp()
 *   3b. = starSurfaceProc()
 *   4b. = golSimulation() -> golLoadRow()
 *   5b. - 7b. =  starSurfaceProc()
 *   8b. = rowReturn()
 *   9b. = starSurfaceProc()
 * 
 * @version 1.0
 * @date 2024-04-08
 */


#include <iostream>
#include <mpi.h>
#include <cstdio>
#include <cmath>
#include <cstdint>
#include <queue>

#include "game_state.hpp"

#define COREPROC 0

/**
 * @brief Enumerator for Infromative messages
 */
enum Status { Start, Pull, Terminate, Ack, AckRequest, Deny };
/**
 * @brief Enumerator for message Tag
 */
enum MessageType { Inform, Value };

/**
 * @brief Structure for sending data chunks to processes
 * - *_no = stores element (row/line) index, indexed from one (unique per transmission)
 * - *_cnt = overall number of elements being sent
 * - value = line value
 * - no_write = how many rows (out of row_cnt) are read only
 * - package_line_len = number of cells within ROW
 */
struct GOLfeed {
    uint64_t value;
    uint8_t package_line_len;
    uint16_t package_no;
    uint16_t package_cnt;
    uint16_t row_no;
    uint16_t row_cnt;
    uint8_t no_write;
};

bool round243 = false;

void starCoreProc(int rounds, int size);
void starSurfaceProc(int rank, int size);


int main(int argc, char *argv[])
{
    int rank = 0, size = 0, rounds = 0;

    if (argc != 2) {
        std::cerr << "ERROR: Program din not receive a required param: N_of_rounds.\n";
        return 1;
    } else {
        rounds = atoi(argv[1]);
        // If atoi returned zero but zero was not the program parameter
        if (!rounds && ((strlen(argv[1]) != 1) && (argv[1][0] != '0'))) {
            std::cerr << "ERROR: Parameter N_of_rounds needs a numerical nonzero value." << std::endl;
            return 1;
        }
    }

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (rank == 0)
        starCoreProc(rounds, size);
    else
        starSurfaceProc(rank, size);

    MPI_Finalize();

    return 0;
}

/**
 * @brief Core process's function to send Terminate message to all calculation processes
 * 
 * @param size Number of processes
 */
void terminateAllSurfaceLevelProcs(int size)
{
    char buff = Status::Terminate;
    for (uint8_t i = 1; i < size; i++)
        MPI_Send(&buff, 1, MPI_CHAR, i, MessageType::Inform, MPI_COMM_WORLD);
    // Every process should respond with an acknowledgement before terminating
    for (uint8_t i = 1; i < size; i++) {
        MPI_Recv(&buff, 1, MPI_CHAR, i, MessageType::Inform, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        // If something other than Ack is received behaviour is undefined
        if (buff != Status::Ack)
            std::cerr << "ERROR: An unknown error has occured. A hard crash may be incomming." << std::endl;
    }
}

/**
 * @brief Contact establishment with surface processes at the start of a round
 * Healthy communication pattern:
 * Core        Surface
 *   |--(Start)-->|
 *   |            |
 *   |<-(AckReq)--|
 * At the end process is requesting data from core process
 * 
 * @param size Number of processes
 * @return Status
 */
bool startUp(int size)
{
    char buff = Status::Start;
    for (uint8_t i = 1; i < size; i++) {
        MPI_Send(&buff, 1, MPI_CHAR, i, MessageType::Inform, MPI_COMM_WORLD);
    }
    for (uint8_t i = 1; i < size; i++) {
        MPI_Recv(&buff, 1, MPI_CHAR, i, MessageType::Inform, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        if (buff != Status::AckRequest) {
            std::cerr << "ERROR: An unknown error has occured. A hard crash may be incomming." << std::endl;
            return false;
        }
    }
    return true;
}

/**
 * @brief Core process sending data to surface processes
 * 
 * @param gs Game State
 * @param ndata No of rows a process receives
 * @param nowrite How many rows are read only
 * @param recipient Rank of receiving process
 * @param rowsRead How many rows have been sent so far (acts as offset - should be zero at the start)
 */
void sendData(GameState* gs, uint8_t ndata, uint8_t nowrite, uint16_t recipient, uint16_t* rowsRead)
{
    GOLfeed message;
    line* row;

    for (uint16_t j = 0; j < ndata; j++) {
        row = gs->getRow(*rowsRead);
        // Send Values within a row 'j'
        // based on proc index 'i' send additional rows
        uint16_t lineSegments = (row->nelems / BITS) + 1;
        // Fill message with appropriate identification
        for (uint16_t k = 0; k < lineSegments; k++) {
            message.package_no = k + 1;
            message.package_cnt = lineSegments;
            message.package_line_len = row->nelems;
            message.row_cnt = ndata;
            message.row_no = j + 1;
            message.value = row->lineArr[k];
            message.no_write = nowrite;
            MPI_Send(&message, sizeof(GOLfeed), MPI_BYTE, recipient, MessageType::Value, MPI_COMM_WORLD);
        }
        // Adjust sent row counter
        // For last send for each recipient decrement offset - account for read-only row
        if (j == (ndata - 1)) {
            (*rowsRead)--;
            continue;
        }
        (*rowsRead)++;
    }
}

/**
 * @brief Core process receives data from surface processes
 * 
 * @param gs Game State
 * @param amnt Expected number of rows to be received from recipient
 * @param recipient Rank of sender
 * @param rowsRead persistent offset counter (should be initialized to zero)
 * @return Status
 */
bool receiveData(GameState* gs, uint8_t amnt, uint8_t recipient, uint16_t* rowsRead)
{
    GOLfeed message;
    
    // Send pull signal
    uint8_t buff = Status::Pull, res;
    MPI_Send(&buff, 1, MPI_CHAR, recipient, MessageType::Inform, MPI_COMM_WORLD);
    MPI_Recv(&res, 1, MPI_CHAR, recipient, MessageType::Inform, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    if (res == Status::Terminate)
        return false;

    buff = Status::Ack;
    MPI_Send(&buff, 1, MPI_CHAR, recipient, MessageType::Inform, MPI_COMM_WORLD);
    // If received Deny, skip to the end
    if (res == Status::Deny)
        return true;
    line* row = nullptr;
    uint16_t rowSegments = (gs->getRow(0)->nelems / BITS) + 1;
    for (uint j = 0; j < amnt * rowSegments; j++) {
        fflush(stdout);
        MPI_Recv(&message, sizeof(GOLfeed), MPI_BYTE, recipient, MessageType::Value, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        // Update values in GameState based on value metadata
        row = gs->getRow(*rowsRead + message.row_no - 1);
        row->lineArr[message.package_no - 1] = message.value;
        row->nelems = message.package_line_len;
    }
    // Adjust row offset / row counter
    (*rowsRead) += message.row_cnt;
    buff = Status::Ack;
    MPI_Send(&buff, 1, MPI_CHAR, recipient, MessageType::Inform, MPI_COMM_WORLD);
    return true;
}

/**
 * @brief Core proc's actions within a single round
 * 
 * @param gs Game State
 * @param surfacePeaks Number of surface processes (peaks because of star topology)
 * @param distrib I/O array recording which row has been processed by which process
 * @return Status
 */
bool simulateRound(GameState* gs, uint8_t surfacePeaks, uint8_t* distrib)
{
    // Preprocess input to optimal rectangle
    if (!gs->prePorcessForIteration())
        return false;
    // Contact Surface Nodes, inform about startup
    if (!startUp(surfacePeaks + 1))
        return false;
    // Received AckRequest from all
    
    // Divide rows to processes
    // To unused, send Deny
    uint16_t rowsToEach = (gs->getNRows() / surfacePeaks);
    uint8_t remainder = ((gs->getNRows() - (rowsToEach * surfacePeaks)) % surfacePeaks), amnt = 0;
    uint16_t rread = 0;

    // Each process receives rowsToEach rows (+ 1 if necessary)
    for (uint8_t i = 1; i < surfacePeaks + 1; i++) {
        if (remainder) {
            remainder--;
            // If remainder is nonzero, incorporate one to rows for proc 'i'
            amnt = rowsToEach + 1;
        } else {
            amnt = rowsToEach;
        }
        // Addition denotes the additional rows a process needs to receive to update it's rows
        // The additional rows will not be changed
        uint8_t additional = 0;
        if (surfacePeaks > 1)
            additional += ((i == 1) || (!(rowsToEach + remainder)) || (i == surfacePeaks)) ? 1 : 2;
        
        uint8_t buff = Status::Ack;
        if (amnt == 0)
            buff = Status::Deny;
        MPI_Send(&buff, 1, MPI_CHAR, i, MessageType::Inform, MPI_COMM_WORLD);
        if (buff != Status::Deny)
            sendData(gs, amnt + additional, additional, i, &rread);
    }
    // Retrieve results from processes
    // Processes are expected to return only the changed rows
    // Refresh remainder value
    remainder = gs->getNRows() % surfacePeaks;
    rread = 0;
    uint16_t distribIdx = 0;
    for (uint8_t i = 1; i < surfacePeaks + 1; i++) {
        if (remainder) {
            remainder--;
            // If remainder is nonzero, incorporate one to rows for proc 'i'
            amnt = rowsToEach + 1;
        } else {
            amnt = rowsToEach;
        }
        // Setting distribution array to proc ranks
        if (distrib) {
            for (uint16_t j = distribIdx; j < distribIdx + amnt; j++)
                distrib[j] = i;
            distribIdx += amnt;
        }
        // Receive data from surface proc i
        if (!receiveData(gs, amnt, i, &rread))
            return false;
    }
    // Round of GOL is over
    return true;
}

/**
 * @brief Behaviour of the Core process
 * 
 * @param rounds No of rounds to be simulated
 * @param size No of processes
 */
void starCoreProc(int rounds, int size)
{
    // Loads, stores and writes game state
    GOLfeed message = { 0 };
    uint8_t* procDistrib = nullptr;
    GameState* gs = new GameState();
    if (!gs->initGameState()) {
        terminateAllSurfaceLevelProcs(size);
        return;
    }
    // Load Game State from stdin
    // Program demands for the test script to cat and or pipe the input file to stdin
    if (!gs->loadGameState(stdin)) {
        terminateAllSurfaceLevelProcs(size);
        return;
    }
    uint8_t surface_peaks = size - 1;
    //  >= size indicates unsigned integer underflow
    if ((surface_peaks == 0) || (surface_peaks >= size)) {
        std::cerr << "ERROR: The Program requires at least 2 processes to function.\n*HINT*: The most efficient number is one process per row of game." << std::endl;
        terminateAllSurfaceLevelProcs(size);
        return;
    }

    bool correctFlag = true;
    for (int rnd = 0; rnd < rounds; rnd++) {
        if (rnd == 242)
            round243 = true;
        else
            round243 = false;
        // Resize proc ristribution to fit all in current round rows
        if ((rnd + 1) == rounds)
            procDistrib = (uint8_t*) malloc(gs->getNRows() * sizeof(uint8_t));
        // Round Simulation
        if (!simulateRound(gs, surface_peaks, procDistrib)) {
            correctFlag = false;
            break;
        }
    }
    // Upon completion terminate all surfacec processes
    terminateAllSurfaceLevelProcs(size);
    if (correctFlag) {
        // If no error occured print resulting Game State
        gs->printGameState(procDistrib);
        if (procDistrib)
            free(procDistrib);
    }
    delete gs;
}

/**
 * @brief Surface processes' replies to round startup
 * 
 * @return true = Continue operation
 * @return false = Terminate
 */
bool surfaceStartUp()
{
    uint8_t buff;
    MPI_Recv(&buff, 1, MPI_CHAR, COREPROC, MessageType::Inform, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    if (buff == Status::Terminate) {
        buff = Status::Ack;
        // Mandatory Ack signal on terminate
        MPI_Send(&buff, 1, MPI_CHAR, COREPROC, MessageType::Inform, MPI_COMM_WORLD);
        return false;
    }
    // Inquire about possible work 
    buff = Status::AckRequest;
    MPI_Send(&buff, 1, MPI_CHAR, COREPROC, MessageType::Inform, MPI_COMM_WORLD);
    return true;
}

/**
 * @brief Receive rows from core process
 * 
 * @param rows I/O uninitialized pointer to line* used to store given rows
 * @param nrows I/O number of rows received
 * @param nowrite I/O number of read-only rows
 * @return Status
 */
bool golLoadRow(line** rows, uint16_t* nrows, uint8_t* nowrite)
{
    bool error = false;
    GOLfeed message;
    line* row = nullptr;
    bool fst = true;
    // Receive messages and alloc arrays
    while (true) {
        // Terminating condition
        if (!fst) {
            if ((message.row_no == message.row_cnt) && (message.package_no == message.package_cnt))
                break;
        } else {
            fst = false;
        }
        MPI_Recv(&message, sizeof(GOLfeed), MPI_BYTE, 0, MessageType::Value, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        // Error occured but keep reading in order not to block sender
        if (error)
            continue;
        if ((message.row_no == 1) && (message.package_no == 1)) {
            // Preallocate space for rows
            row = (line*) malloc(message.row_cnt * sizeof(line));
            if (!row) {
                error = true;
                fprintf(stderr, "ERROR: MEMALLOC failed.\n");
                continue;
            }
            for (uint8_t t = 0; t < message.row_cnt; t++)
                row[t].lineArr = nullptr;
            *rows = row;
            *nrows = message.row_cnt;
            *nowrite = message.no_write;
        }
        if (message.package_no == 1) {
            row[message.row_no - 1].lineArr = (uint64_t*) malloc(message.package_cnt * sizeof(uint64_t));
            if (!(row[message.package_no - 1]).lineArr) {
                error = true;
                fprintf(stderr, "ERROR: MEMALLOC failed.\n");
                continue;
            }
        }
        // Load message deets
        row[message.row_no - 1].nelems = message.package_line_len;
        row[message.row_no - 1].lineArr[message.package_no - 1] = message.value;
    }
    // free resources and set to null
    if (error) {
        if (*rows)
            for (uint8_t t = 0; t < *nrows; t++) {
                if ((*rows)[t].lineArr)
                    free((*rows)[t].lineArr);
            free(*rows);
            *rows = nullptr;
        }
    }
    return !error;
}

/**
 * @brief Partial Game of Life simulation by surface processes
 * 
 * @param rows I/O received rows
 * @param nrows I/O number of rows received
 * @param nowrite I/O number of read-only rows
 * @param rankOne Flag is true only for process with Game State row zero => has no read-only row above
 * @return Status
 */
bool golSimulation(line** rows, uint16_t* nrows, uint8_t* nowrite, bool rankOne)
{
    // Receive rows from Core
    if (!golLoadRow(rows, nrows, nowrite))
        return false;

    // Perform step
    uint8_t startIdx, endIdx;
    if (rankOne) {
        startIdx = 0;
        endIdx = *nrows - *nowrite;
    } else {
        startIdx = 1;
        endIdx = (*nowrite == 1) ? *nrows : *nrows - 1;
    }
    // Create new temporary rows in order not to taint output
    line* tmpRows = (line*) malloc(*nrows * sizeof(line));
    if (!tmpRows) {
        fprintf(stderr, "ERROR: MEMALLOC failed.\n");
        // Free rows
        for (uint8_t t = 0; t < *nrows; t++)
            if ((*rows)[t].lineArr)
                free((*rows)[t].lineArr);
        free(*rows);
        return false;
    }
    for (uint16_t i = 0; i < *nrows; i++) {
        tmpRows[i].nelems = (*rows)[i].nelems;
        tmpRows[i].lineArr = (uint64_t*) malloc(((tmpRows[i].nelems / BITS) + 1) * sizeof(uint64_t));
        if (!tmpRows[i].lineArr) {
            fprintf(stderr, "ERROR: MEMALLOC failed.\n");
            // There will be a leak
            free(tmpRows);
            // free original rows
            for (uint8_t t = 0; t < *nrows; t++)
                if ((*rows)[t].lineArr)
                    free((*rows)[t].lineArr);
            free(*rows);
            return false;
        }
        for (uint16_t j = 0; j < (tmpRows[i].nelems / BITS) + 1; j++) {
            tmpRows[i].lineArr[j] = (*rows)[i].lineArr[j];
        }
    }
    // Use tmpRows to read and store changes to *rows
    for (uint16_t i = startIdx; i < endIdx; i++) {
        for (uint16_t j = 0; j < (tmpRows)[i].nelems; j++) {
            // Chech Moore Distance for alive cells
            uint8_t aliveCells = 0;
            for (int8_t x = (int8_t)i - 1; x < i + 2; x++) {
                for (int8_t y = (int8_t)j - 1; y < j + 2; y++) {
                    if ((x == i) && (y == j))
                        continue; 
                    if ((x >= 0) && (x < *nrows) && (y >= 0) && (y < (*rows)[i].nelems))
                        if (isAlive((tmpRows)[x].lineArr[y / BITS], y % BITS))
                            aliveCells++;
                }
            }

            // Evaluate for next state
            // Due to the existance of temporary rows, we can write directly to received rows
            if (isAlive((*rows)[i].lineArr[j / BITS], j % BITS)) {
                if ((aliveCells < 2) || (aliveCells > 3)) {
                    registerDead(&((*rows)[i].lineArr[j / BITS]), j % BITS);
                }
            } else {
                if (aliveCells == 3) {
                    registerAlive(&((*rows)[i].lineArr[j / BITS]), j % BITS);
                }
            }
        }
    }
    // free tmpRows
    for (uint16_t i = 0; i < *nrows; i++)
        free(tmpRows[i].lineArr);
    free(tmpRows);
    return true;
}

/**
 * @brief Send altered rows back to Core process
 * 
 * @param rows I/O rows to be sent (memory is then freed)
 * @param nrows Number of rows (including read-only)
 * @param nowrite Number of read-only rows
 * @param rankOne Flag is true for surface process with the Game State's row zero
 */
void rowReturn(line** rows, uint16_t nrows, uint8_t nowrite, bool rankOne)
{
    GOLfeed message;
    uint16_t packCnt, startIdx, endIdx;
    // Determine which rows to send by nowrite and flag rankOne
    if (rankOne) {
        startIdx = 0;
        endIdx = nrows - nowrite;
    } else {
        startIdx = 1;
        endIdx = (nowrite == 1) ? nrows : nrows - 1;
    }
    // Send rows in calculated interval
    for (uint16_t i = startIdx; i < endIdx; i++) {
        packCnt = ((*rows)[i].nelems / BITS) + 1;
        for (uint16_t j = 0; j < packCnt; j++) {
            message.package_cnt = packCnt;
            message.package_no = j + 1;
            message.row_cnt = nrows - nowrite;
            message.row_no = i + 1 - startIdx;
            message.value = (*rows)[i].lineArr[j];
            message.package_line_len = (*rows)[i].nelems;

            MPI_Send(&message, sizeof(GOLfeed), MPI_BYTE, 0, MessageType::Value, MPI_COMM_WORLD);
        }
    }

    // Rows successfully returned. Memspace can be freed
    for (uint16_t i = 0; i < nrows; i++) {
        free((*rows)[i].lineArr);
    }
    free(*rows);
    *rows = nullptr;
}

/**
 * @brief Notify Core of Termination
 * Reply is not important
 */
void forceEnd()
{
    uint8_t buff = Status::Terminate;
    MPI_Send(&buff, 1, MPI_CHAR, COREPROC, MessageType::Inform, MPI_COMM_WORLD);
    MPI_Recv(&buff, 1, MPI_CHAR, COREPROC, MessageType::Inform, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
}

/**
 * @brief Behaviour describing surface processes
 * 
 * @param rank Rank
 * @param size No of Processes
 */
void starSurfaceProc(int rank, int size)
{
    char buff;
    bool denied, error = false;
    // Loop until not Terminated
    do {
        denied = false;
        // Handle startup with Core
        if (!surfaceStartUp())
            break;
        // Continue business as usual
        MPI_Recv(&buff, 1, MPI_CHAR, COREPROC, MessageType::Inform, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        // Denied processes skip the iteration and wait for data request
        line* rows = nullptr;
        uint16_t nrows;
        uint8_t nowrite;
        if (buff == Status::Ack) {
            // Receive data and simulate
            if (!golSimulation(&rows, &nrows, &nowrite, (rank == 1)))
                error = true;
        } else {
            // Process is denied (there is not enough rows)
            denied = true;
        }
        // Receive message from Core (Pull expected - request for changed data)
        MPI_Recv(&buff, 1, MPI_CHAR, COREPROC, MessageType::Inform, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        if (buff != Status::Pull) {
            // Did not receive pull, notify Core of termination
            forceEnd();
            break;
        }
        buff = Status::Ack;
        if (denied)
            buff = Status::Deny;
        if (error)
            buff = Status::Terminate;
        // Send either Ack - if proc received rows, Deny - if proc was denied, Terminate - if error occured
        MPI_Send(&buff, 1, MPI_CHAR, COREPROC, MessageType::Inform, MPI_COMM_WORLD);
        // Receive Ack, if received Terminate (anything other than Ack means Terminate) terminate
        MPI_Recv(&buff, 1, MPI_CHAR, COREPROC, MessageType::Inform, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        // Act accordingly
        if (buff != Status::Ack)
            break;
        if (!denied) {
            rowReturn(&rows, nrows, nowrite, (rank == 1));
            // Receive Ack from Core - Core received all
            MPI_Recv(&buff, 1, MPI_CHAR, COREPROC, MessageType::Inform, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        // Prepare for startup
    } while(true);

    return;
}