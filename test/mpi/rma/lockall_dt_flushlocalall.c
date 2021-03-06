/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 *
 *  (C) 2015 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mpitest.h"

/*
static char MTEST_Descrip[] = "Test for streaming ACC-like operations with lock_all+flush_local_all";
*/

int main(int argc, char *argv[])
{
    int errs = 0;
    int rank, size;
    int minsize = 2, count;
    MPI_Comm comm;
    MPI_Win win;
    MPI_Aint lb, extent;
    MTestDatatype sendtype, recvtype;

    MTest_Init(&argc, &argv);

    while (MTestGetIntracommGeneral(&comm, minsize, 1)) {
        if (comm == MPI_COMM_NULL)
            continue;

        MPI_Comm_rank(comm, &rank);
        MPI_Comm_size(comm, &size);
        int source = 0;

        MTEST_DATATYPE_FOR_EACH_COUNT(count) {
            while (MTestGetDatatypes(&sendtype, &recvtype, count)) {
                recvtype.printErrors = 1;
                recvtype.InitBuf(&recvtype);
                MPI_Type_get_extent(recvtype.datatype, &lb, &extent);

                MPI_Win_create(recvtype.buf, lb + recvtype.count * extent,
                               (int) extent, MPI_INFO_NULL, comm, &win);
                if (rank == source) {
                    int dest;
                    MPI_Aint slb, sextent;
                    MPI_Type_get_extent(sendtype.datatype, &slb, &sextent);
                    sendtype.InitBuf(&sendtype);

                    MPI_Win_lock_all(0, win);
                    for (dest = 0; dest < size; dest++)
                        if (dest != source) {
                            MPI_Accumulate(sendtype.buf, sendtype.count,
                                           sendtype.datatype, dest, 0,
                                           recvtype.count, recvtype.datatype, MPI_REPLACE, win);
                        }

                    MPI_Win_flush_local_all(win);
                    /* reset the send buffer to test local completion */
                    memset(sendtype.buf, 0, slb + sextent * sendtype.count);
                    MPI_Win_unlock_all(win);
                    MPI_Barrier(comm);

                    sendtype.InitBuf(&sendtype);
                    char *resbuf = (char *) calloc(lb + extent * recvtype.count, sizeof(char));

                    /*wait for the destination to finish checking and reinitializing the buffer */
                    MPI_Barrier(comm);

                    MPI_Win_lock_all(0, win);
                    for (dest = 0; dest < size; dest++)
                        if (dest != source) {
                            MPI_Get_accumulate(sendtype.buf, sendtype.count,
                                               sendtype.datatype, resbuf, recvtype.count,
                                               recvtype.datatype, dest, 0, recvtype.count,
                                               recvtype.datatype, MPI_REPLACE, win);
                        }
                    MPI_Win_flush_local_all(win);
                    /* reset the send buffer to test local completion */
                    memset(sendtype.buf, 0, slb + sextent * sendtype.count);
                    MPI_Win_unlock_all(win);
                    MPI_Barrier(comm);
                    free(resbuf);
                } else {
                    int err;
                    MPI_Barrier(comm);
                    MPI_Win_lock(MPI_LOCK_SHARED, rank, 0, win);
                    err = MTestCheckRecv(0, &recvtype);
                    if (err)
                        errs++;
                    recvtype.InitBuf(&recvtype);
                    MPI_Win_unlock(rank, win);

                    /*signal the source that checking and reinitialization is done */
                    MPI_Barrier(comm);

                    MPI_Barrier(comm);
                    MPI_Win_lock(MPI_LOCK_SHARED, rank, 0, win);
                    err = MTestCheckRecv(0, &recvtype);
                    if (err)
                        errs++;
                    MPI_Win_unlock(rank, win);
                }

                MPI_Win_free(&win);
                MTestFreeDatatype(&sendtype);
                MTestFreeDatatype(&recvtype);
            }
        }
        MTestFreeComm(&comm);
    }
    MTest_Finalize(errs);
    return MTestReturnValue(errs);
}
