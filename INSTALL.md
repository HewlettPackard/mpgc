===============================================================================
      Multi Process Garbage Collector
      Copyright © 2016 Hewlett Packard Enterprise Development Company LP.
===============================================================================

# INSTALL instructions for Multi Process Garbage Collector (MPGC)

Author: Susan Spence (susan.spence@hpe.com)

## Prerequisites

64-bit, Linux, C++ 14, pthreads

- g++ 4.9.2, or g++ 5.4.0 or later    
  Note that MDS is known **not** to work with gcc 5.0, because of bugs 
  in that version related to compilation of templates, which we use extensively.

- Multi Process Garbage Collector (MPGC)

## Installations tested

* Linux: Debian Jessie (8.6), 
Ubuntu Xenial Xerus (16.04.1 LTS) on x86_64 architecture, 
[linux-l4fame](https://github.com/FabricAttachedMemory/linux-l4fame)

* g++ 4.9.2 /usr/local/gcc-4.9.2/bin/gcc

    * Compiled from: source downloaded from mirror via https://gcc.gnu.org/

* MPGC: https://github.com/HewlettPackard/mpgc

MPGC runs on 64-bit Linux; it has been tested on x86 and ARM architectures.

NB: MPGC won't run on a virtual machine, 
if the virtual machine doesn't support a shared mmap file.


## Build process

MPGC can be used standalone, or as part of the Managed Data Structures (MDS) library.  

For instructions on compiling MPGC as part of MDS, see:  
    [https://github.com/HewlettPackard/mds/blob/master/INSTALL.md](https://github.com/HewlettPackard/mds/blob/master/INSTALL.md)

To compile sources for MPGC standalone:
- to build optimized, build under mpgc/build/intel-opt; 
- to build debug, build under mpgc/build/intel-debug; e.g.

    ```
    cd mpgc/build/intel-opt    
    make
    ```

The build will take a couple of minutes to complete on an average Linux server.
    
The build process compiles:
- some commonly used MDS project code into libruts.a;
- the code for MPGC into libmpgc.a; 
- the code for MPGC tests and tools.

The code common to both MDS and MPGC is compiled to libruts.a:
where "ruts" stands for "Really Useful ToolS".

To see if the build has completed successfully, 
under the directory where the build was invoked, 
check for the existence of the following files: 

    libs/libmpgc.a    
    tools/createheap
    tools/gcdemo

If you have a problem, and need to clean the build and start again:    

    cd mpgc/build/intel-opt
    make clean-recursive


## MPGC Demo  

This demo creates and populates a social network, 
then runs a demo program that simulates use of this network, 
including users making posts, adding comments to existing posts, 
and tagging users in posts. 

1. Set up the MPGC Demo - by creating a symbolic link 
to the Python script that runs the demo:

    ```
    cd mpgc/build/intel-opt/tools    
    ln -s ../../../tools/rungcdemo/rungcdemo.py
    ```

2. Create heap - creating it in /dev/shm - specifying a size of 1 GB

    ```
    mkdir /dev/shm/mpgc_heaps    
    ln -s /dev/shm/mpgc_heaps heaps    
    ./createheap 1G
    ```

3. Run GC demo with specified parameters: 

    ```
    ./rungcdemo.py -b -u 1000 -i 5000 -w 100000    
    ```

    -b - runs the demo in benchmark mode    
    -u 1000 - generates a graph with 1000 users     
    -i 5000 - runs benchmark for 5000 iterations    
    -w 100,000 - with a workrate targeting 100,000 transactions per second    


4. To see the full list of all flags available for this demo, use -h option: 

    ```
    ./rungcdemo.py -h 
    ```



