/*
 * Copyright (c) 2020-2021  Christoph Niethammer <niethammer@hlrs.de>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER NOR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <inttypes.h>
#include <mpi.h>
#include <numa.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "config.h"

int test_p2p_connectivity(MPI_Comm comm) {
    int ret = MPI_SUCCESS;
    int comm_size = 1;
    int comm_rank = 0;
    MPI_Comm_size(comm, &comm_size);
    MPI_Comm_rank(comm, &comm_rank);

    if (comm_size <= 1) {
        fprintf(
            stderr,
            "[mpi-p2p-checker] %d/%d skipping as communicator is too small\n",
            comm_rank, comm_size);
        return MPI_SUCCESS;
    }

    double p2p_times[comm_size];
    for (int i = 0; i < comm_size; i++) {
        p2p_times[i] = 0.0;
    }

    /* Perform communication test between all ranks using a round based pattern
     * as it is used to orgranize e.g. chess turnaments. place in two rows
     * facing each other. Per round shift all except rank 0 .E.g. for 10
     * processes:
     *
     * round 1:
     *  send first: 0 1 2 3 4
     *  recv first: 9 8 7 6 5
     * round 2:
     *  send first: 0 9 1 2 3
     *  recv first: 8 7 6 5 4
     * For odd number add one round and let one rank pause starting with rank 0.
     */
    int num_rounds = comm_size - 1 + (comm_size % 2);
    int partners[num_rounds + 1];
    for (int i = 0; i < num_rounds + 1; i++) {
        partners[i] = i;
    }

    for (int round = 0; round < num_rounds; round++) {
        int send_buffer = comm_rank;
        int recv_buffer = -1;
        MPI_Datatype datatype = MPI_INT;
        int tag = 0;

        double start_wtime;
        double end1_wtime;
        double end2_wtime;

        int pos = 0;
        if (comm_rank > 0) {
            pos = 1 + (comm_rank - 1 + round) % num_rounds;
        }
        int partner = partners[num_rounds - pos];
        bool send_first = pos < (num_rounds + 1) / 2;
        if (send_first) {
            MPI_Send(&send_buffer, 1, datatype, partner, tag, comm);
            MPI_Recv(&recv_buffer, 1, datatype, partner, tag, comm,
                     MPI_STATUS_IGNORE);
            start_wtime = MPI_Wtime();
            MPI_Send(&send_buffer, 1, datatype, partner, tag, comm);
            MPI_Recv(&recv_buffer, 1, datatype, partner, tag, comm,
                     MPI_STATUS_IGNORE);
            end1_wtime = MPI_Wtime();
            end2_wtime = MPI_Wtime();
        } else {
            MPI_Recv(&recv_buffer, 1, datatype, partner, tag, comm,
                     MPI_STATUS_IGNORE);
            MPI_Send(&send_buffer, 1, datatype, partner, tag, comm);
            start_wtime = MPI_Wtime();
            MPI_Recv(&recv_buffer, 1, datatype, partner, tag, comm,
                     MPI_STATUS_IGNORE);
            MPI_Send(&send_buffer, 1, datatype, partner, tag, comm);
            end1_wtime = MPI_Wtime();
            end2_wtime = MPI_Wtime();
        }
        p2p_times[partner] = 2.0 * end1_wtime - start_wtime - end2_wtime;

        printf("[mpi-p2p-checker] Round %d: %d <--> %d: %lf\n", round,
               comm_rank, partner, p2p_times[partner]);

        /* we are switching to the other side in the net round */
        if (partner == 0) {
            send_first = true; /* becoming sender */
        }
        if (partner == comm_rank + 1 ||
            (partner == 1 && comm_rank == num_rounds)) {
            send_first = false; /* becoming receiver */
        }

        /* rotate group of partners to right except 0 */
        int tmp = partners[num_rounds];
        for (int i = num_rounds; i > 1; i--) {
            partners[i] = partners[i - 1];
        }
        partners[1] = tmp;
    }
    return ret;
}

int MPI_Init(int *argc, char **argv[]) {
    int ret = PMPI_Init(argc, argv);
    MPI_Comm comm = MPI_COMM_WORLD;
    int comm_size = 1;
    int comm_rank = 0;
    MPI_Comm_size(comm, &comm_size);
    MPI_Comm_rank(comm, &comm_rank);

    /* Get a approximately synchronized clock for all processes */
    int msg_size = 1;
    if (comm_rank == 0) {
        printf("[mpi-p2p-checker] Running %s version %s\n", PACKAGE_NAME,
               PACKAGE_VERSION);
        printf("[mpi-p2p-checker] \n");
        printf("[mpi-p2p-checker] message size: %lu bytes\n",
               msg_size * sizeof(long));
        printf(
            "[mpi-p2p-checker] checking P2P connectivity between all "
            "sockets\n");
        time_t now;
        time(&now);
        printf("[mpi-p2p-checker] start date: %s\n", ctime(&now));
    }

    /* Create a shared communicator, which we can then split further to NUMA
     * nodes (sockets) */
    MPI_Comm comm_shared;
    MPI_Comm_split_type(comm, MPI_COMM_TYPE_SHARED, comm_rank, MPI_INFO_NULL,
                        &comm_shared);
    int comm_shared_rank = 0;
    MPI_Comm_rank(comm_shared, &comm_shared_rank);

    /* split shared communicator into numa node (socket) communicator */
    /* start determining the numa node/socket id for this MPI process */
    numa_available();
    struct bitmask *numa_mask = numa_get_run_node_mask();
    int num_nodes = numa_num_configured_nodes();
    int num_nodes_for_rank = 0;
    int node_id = -1;
    for (int i = 0; i < num_nodes; i++) {
        if (numa_bitmask_isbitset(numa_mask, i)) {
            node_id = i;
            num_nodes_for_rank++;
        }
    }
    if (num_nodes_for_rank != 1) {
        fprintf(stderr,
                "[mpi-p2p-checker] Error: MPI P2P check library cannot run if "
                "processes are "
                "allowed to migrate between sockets\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    MPI_Comm comm_socket; /* communicator with all ranks from comm_shared within
                             a socket */
    MPI_Comm_split(comm_shared, node_id, comm_shared_rank, &comm_socket);

    /* now create a communicator across nodes with maximal one process per
     * socket */
    /* note there exists later on multiple communicators for the different */
    int comm_socket_rank = 0;
    MPI_Comm_rank(comm_socket, &comm_socket_rank);
    MPI_Comm comm_sockets; /* communicator with one rank per socket from comm */
    MPI_Comm_split(comm, comm_socket_rank, comm_rank, &comm_sockets);

    MPI_Barrier(MPI_COMM_WORLD);
    double start_wtime = MPI_Wtime();

    if (comm_socket_rank == 0) {
        test_p2p_connectivity(comm_sockets);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double end_wtime = MPI_Wtime();
    double duration = end_wtime - start_wtime;

    if (comm_rank == 0) {
        fprintf(stdout,
                "[mpi-p2p-checker] MPI P2P %s connectivity test took: %lf "
                "seconds\n",
                "node", duration);
    }

    return ret;
}

int MPI_Finalize() {
    int comm_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &comm_rank);
    if (comm_rank == 0) {
        time_t now;
        time(&now);
        printf("[mpi-p2p-checker] end date: %s\n", ctime(&now));
    }
    return PMPI_Finalize();
}
