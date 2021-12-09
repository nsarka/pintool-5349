# pintool-5349

## Setup
- Open the setup.sh script and change the variables to your installations of Corundum-extensions and Pin
- Run ./setup.sh

## Running
- Edit the run.sh script similarly to the setup.sh script
- Run ./run.sh

## Sample build command without using the script

```
PIN_ROOT=/optane/sarkauskas.1/pin make
```

## Sample run command without using the script

```
PIN_ROOT=/optane/sarkauskas.1/pin /optane/sarkauskas.1/pin/pin -t ./obj-intel64/pmem.so -- /path/to/binary
```

The pintool depends on a patch to write all opened persistent memory files to /tmp/pmem.txt. The pintool 
will call a function to read and then delete the file after BuddyAllocator::open_impl finishes executing.
After reading the addresses in /tmp/pmem.txt, it will add them to a set of pair<start_address, end_address>.

The pintool hooks every memory write, checks to see if it's within any of the ranges stored as pairs in the set,
and if it is, it will:
 - Get the start of the cache line of this address
 - Check if the cache line address is stored already in the map
 - If it's not, insert the address and the backtrace to the map
 - If it is, then erase that entry (actually, this step has a slight issue: we cant detect if the previous 
   write was on the same cache line but on a different byte. Before I changed it to work at the cache-line level,
   it would print backtraces of addresses that were overwritten without being flushed first. After changing it
   to work at the cache line level, I commented this print (line 178)), and then store the new backtrace

On every clflush, clflushopt, and clwb:
 - Get the cache line start of the address being flushed
 - Erase the entry in the map with this cache line
 - If there was a clflush on an address without a write, print a warning

After the application finishes:
 - Print pmem address ranges
 - Print the flush count total
 - Print all map entries
