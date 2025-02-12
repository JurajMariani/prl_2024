/**
 * @file pms.cpp
 * @author Bc. Juraj Mariani, xmaria03 <xmaria03@stud.fit.vutbr.cz>
 * @brief Pipeline merge sort implementation using OpneMPI
 * @version 1.0
 * @date 2024-04-08
 */

#include <iostream>
#include <mpi.h>
#include <cstdio>
#include <cmath>
#include <queue>

/**
 * @brief NumberPMS is a message structure
 * Contains:
 *      - bool up :                 determines which queue should the element be a part of
 *      - bool endOfStream:         informs the process that current value received is the last of stream - e.i. last to enter up queue/down queue depending on bool up
 *      - bool endOfTransmission:   informs the process that current value is the last value to be processed
 */
struct NumberPMS {
    uint8_t value;
    bool up;
    bool endOfStream;
    bool endOfTransmission = false;
};

void layerOneProc(FILE*);
void layerNProc(int, int);


int main(int argc, char *argv[])
{
    int rank = 0, size = 0;
    NumberPMS message;

    // File descriptor is opened before processes are created
    FILE* f = fopen("numbers", "r");
    if (!f) {
        std::cerr << "Error: Can't open numbers." << std::endl;
        return 1;
    }

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (rank == 0)
        layerOneProc(f);
    else
        layerNProc(rank, size);

    MPI_Finalize();
    fclose(f);

    return 0;
}

/**
 * @brief Function for Processes of rank 1 and higher
 *        This function implements the sorting
 * 
 * @param rank Rank of the process
 * @param size Size of process space
 */
void layerNProc(int rank, int size)
{
    // Along with input queues an output queue is created for ease of use
    std::queue<uint8_t> upQueue, downQueue, resultQueue;
    // Flag of last process in chain
    bool last = (rank + 1) == size;
    bool up = true;
    NumberPMS message;

    while (!message.endOfTransmission) {
        MPI_Recv(&message, sizeof(NumberPMS), MPI_BYTE, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        if (message.up)
            upQueue.push(message.value);
        else
            downQueue.push(message.value);

        // Sorting logic starts here
        if ((((upQueue.size() >= std::pow(2, (rank - 1))) && (downQueue.size() >= 1)) && message.endOfStream) || message.endOfTransmission) {
            // Enough items to sort (if the sequence did not end)
            // or end of transmission means flood input queues to output
            while ((!upQueue.empty()) && (!downQueue.empty())) {
                if (upQueue.front() < downQueue.front()) {
                    resultQueue.push(upQueue.front());
                    upQueue.pop();
                } else {
                    resultQueue.push(downQueue.front());
                    downQueue.pop();
                }
            }
            std::queue<uint8_t>* residue;
            residue = ((upQueue.empty()) ? &downQueue : &upQueue);

            // Append the unempty queue to result
            while (!residue->empty()) {
                resultQueue.push(residue->front());
                residue->pop();
            }

            bool eot = message.endOfTransmission;
            // Either send sequence to next process in the pipeline or, in case of the last process, print sequence to stdout
            while (!resultQueue.empty()) {
                if (last) {
                    std::cout << static_cast<int>(resultQueue.front()) << std::endl;
                    std::cout.flush();
                    resultQueue.pop();
                } else {
                    message.value = resultQueue.front();
                    message.up = up;
                    message.endOfStream = (resultQueue.size() == 1);
                    message.endOfTransmission = ((resultQueue.size() == 1) && eot);
                    resultQueue.pop();

                    MPI_Send(&message, sizeof(NumberPMS), MPI_BYTE, rank + 1, 0, MPI_COMM_WORLD);
                }
            }
            // Change the input queue of next process in the pipeline
            up = !up;
        }
    }
}

/**
 * @brief 
 * 
 */
void layerOneProc(FILE* numberFile)
{
    uint16_t num = 0;
    std::queue<uint8_t> inputQeue;
    
    // Read numbers from file descriptor, temporary storage for ease of use only
    // instead of inputQueue, read numbers could have been sent without the queue overhead
    while ((num = fgetc(numberFile)) != (uint16_t)EOF) {
        inputQeue.push((uint8_t)num);
    }

    // Fitst send to the upper queue
    bool up = true;
    NumberPMS message;
    while (!inputQeue.empty()) {
        message.value = inputQeue.front();
        message.up = up;
        message.endOfStream = true;
        message.endOfTransmission = (inputQeue.size() == 1);
        // Print the horizontal sequence of numbers
        std::cout << static_cast<int>(inputQeue.front()) << ((inputQeue.size() == 1) ? "\n" : " ");
        std::cout.flush();

        // Change the receiver's storage queue
        up = !up;
        inputQeue.pop();

        // Send the number
        MPI_Send(&message, sizeof(NumberPMS), MPI_BYTE, 1, 0, MPI_COMM_WORLD);
    }
}
