# MPI P2P checker

Small library to check connectivity of all sockets in a job. The library works
using LD_PRELOAD mechanism and the PMPI interface hooking into MPI_Init to check
P2P communication between all sockets. Result will be send to stdout during
MPI_Finalize.

Note: This test does not do sophisticated statistics to get accurate timing values.

## Getting Started

### Prerequisites

The basic C version of the mpi-p2p-checker library requires
* C99 compatible compiler
* MPI-2 compliant MPI library

### Installing and Usage

To build, install, and use the mpi-p2p-checker library execute
```shell
# In case you build from the source repository run  autoreconf -vif
mkdir build && cd build
./configure CC=mpicc --prefix=$INSTALLPATH ...
make
make install
LD_PRELOAD=$INSTALLPATH/lib/libmpi-p2p-checker.so mpirun <MPI OIPTIONS> <APP>
```

## Contact

Christoph Niethammer <niethammer@hlrs.de>
