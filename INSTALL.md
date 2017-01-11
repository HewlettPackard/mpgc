===============================================================================
      Multi Process Garbage Collector
      Copyright © 2016 Hewlett Packard Enterprise Development Company LP.
===============================================================================

# INSTALL instructions for Multi Process Garbage Collector (MPGC)

Author: Susan Spence (susan.spence@hpe.com)

## Prerequisites

64-bit, Linux, C++ 14, pthreads

- g++ 4.9.2

- Multi Process Garbage Collector (MPGC)

## Installations tested

* g++ 4.9.2 /usr/local/gcc-4.9.2/bin/gcc

    * Compiled from: source downloaded from mirror via https://gcc.gnu.org/

* MPGC: https://github.com/HewlettPackard/mpgc

MPGC runs on 64-bit Linux; it has been tested on x86 and ARM architectures.

## Build process

MPGC can be used standalone, or as part of the Managed Data Structures (MDS) library.  

For instructions on compiling MPGC as part of MDS, see:  
    [https://github.com/HewlettPackard/mds/blob/master/INSTALL.md](https://github.com/HewlettPackard/mds/blob/master/INSTALL.md)

To compile sources for MPGC standalone:

    cd build/intel-opt
    make
    
The build process:
- compiles some commonly used MDS project libraries into libruts.a;
- then compiles the code for MPGC into libmpgc.a.

The code common to both MDS and MPGC is compiled to libruts.a:
where "ruts" stands for "Really Useful ToolS".

The MPGC library libruts.a can be built either optimised or debug.

When built optimized, the .a library is created under
    mpgc/build/intel-opt/libs

When built debug, the .a library is created under
    mpgc/build/intel-debug/libs
    

