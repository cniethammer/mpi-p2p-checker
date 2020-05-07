# MPI P2P checker

Small library to check connectivity of all sockets in a job. The library works
using LD_PRELOAD mechanism and the PMPI interface hooking into MPI_Init to check
P2P communication between all sockets. Result will be send to stdout.
