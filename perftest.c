// Ping-Pong MPI Performance Tester
// written by Bryan Carroll

#define _GNU_SOURCE // for gethostname()

#include "mpi.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>

#define START_SIZE 1000000 // beginning number of elements
#define END_SIZE START_SIZE // end number of elements
#define MULTIPLIER 10 // how fast the array size increases
#define ITERATIONS 30 //how many times to send back and forth, 10 iterations is 20 sends and receives

// message tags; they don't need to be changed
#define MESSAGE_BEGIN 0
#define MESSAGE_TEST 1

// stop is for debugging.
void stop(int processNum) {
	int i = 0;
	char hostname[256];
	gethostname(hostname, sizeof(hostname));
	printf("Hostname: %s Process: %i PID: %i waiting for gdb attach\n", hostname, processNum, getpid());
	fflush(stdout);
	while(0 == i) {
		sleep(5);
	}
}

// measure the bandwidth between two processes
// other is the rank of the other process that is participating in the test
void test(int myRank, int other, bool initiator){

    FILE* file;
    if(initiator){
        file = fopen("perftest.txt", "a");
    }

    for(int size = START_SIZE; size <= END_SIZE; size *= MULTIPLIER){

        int* buffer = (int*)malloc(sizeof(int) * size);
        // doesn't matter what's in the buffer

        // send a null message back and forth so both participating ranks know it's time to start
        if(initiator){
            MPI_Send(NULL, 0, MPI_INT, other, MESSAGE_BEGIN, MPI_COMM_WORLD);
            MPI_Recv(NULL, 0, MPI_INT, other, MESSAGE_BEGIN, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        else{
            int flag = false;
            while(!flag){
                sleep(1);
                MPI_Iprobe(other, MESSAGE_BEGIN, MPI_COMM_WORLD, &flag, MPI_STATUS_IGNORE);
            }
            MPI_Recv(NULL, 0, MPI_INT, other, MESSAGE_BEGIN, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Send(NULL, 0, MPI_INT, other, MESSAGE_BEGIN, MPI_COMM_WORLD);
        }

        double begin;
        if(initiator){
            begin = MPI_Wtime();
        }

        for(int iter = 0; iter < ITERATIONS; iter++){
            if(initiator){
                MPI_Send(buffer, size, MPI_INT, other, MESSAGE_TEST, MPI_COMM_WORLD);
                MPI_Recv(buffer, size, MPI_INT, other, MESSAGE_TEST, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
            else{
                MPI_Recv(buffer, size, MPI_INT, other, MESSAGE_TEST, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Send(buffer, size, MPI_INT, other, MESSAGE_TEST, MPI_COMM_WORLD);
            }
        }

        if(initiator){
            double end = MPI_Wtime();
            double total = end - begin;
            double average = total / ITERATIONS / 2;  // divide by 2 because message is passed back and forth
            double gigabits = sizeof(int) * size * 8 /* 8 bits */ / 1e9 / average;

            fprintf(file, "Between %2i and %2i   Speed: %6.3lf Gb/s   Average: %7.3lf ms   Array Size: %5.3f MB   Elements: %8i\n", myRank, other, gigabits, average * 1000, (float) sizeof(int) * size / 1000000, size);
        }
        free(buffer);
    }

    if(initiator){
        fclose(file);
    }
}

int main(int argc, char *argv[]) {

	MPI_Init(&argc, &argv);
	int mpiSize, rank;
	MPI_Comm_size(MPI_COMM_WORLD, &mpiSize);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    char hostname[256];
	gethostname(hostname, sizeof(hostname));
	printf("Hostname: %s Rank: %i\n", hostname, rank);
    if(0 == rank){
        printf("Iterations: %i\n", ITERATIONS);
        // clear the log file
        FILE* file = fopen("perftest.txt", "w");
        fclose(file);
    }
    fflush(stdout);

    // begin measurement loop
    for(int initiator = 0; initiator < mpiSize - 1; initiator++){
        if(initiator == rank){
            // measure every rank that sequentially after the initiator rank
            for(int destination = initiator + 1; destination < mpiSize; destination++){
                test(rank, destination, true);
            }
        }
        else if(rank > initiator){
            test(rank, initiator, false);
        }
        MPI_Barrier(MPI_COMM_WORLD); // this is to keep the next initiator from opening the file
    }
    
    MPI_Finalize();

    return 0;
}