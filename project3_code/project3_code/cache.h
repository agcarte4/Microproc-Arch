#ifndef CACHE_H_
#define CACHE_H_

#include <stdio.h>
#include <stdbool.h>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>

using namespace std;

#define UNDEFINED 0xFFFFFFFFFFFFFFFF //constant used for initialization

typedef enum {WRITE_BACK, WRITE_THROUGH, WRITE_ALLOCATE, NO_WRITE_ALLOCATE} write_policy_t; 

typedef enum {HIT, MISS} access_type_t;

typedef unsigned long long address_t; //memory address type

typedef struct{
	unsigned lru;	// keeps track of lru
	bool dirty;		// keeps track of valid
	unsigned long long tag;	// the tag
} cache_block_t;

class cache{

	/* Add the data members required by your simulator's implementation here */
	unsigned cache_size; 				// cache size (in bytes)
    unsigned cache_associativity;     	// cache associativity (fully-associative caches not considered)
	unsigned cache_line_size;         	// cache block size (in bytes)
	write_policy_t write_hit_policy;  	// write-back or write-through
	write_policy_t write_miss_policy; 	// write-allocate or no-write-allocate
	unsigned cache_hit_time;			// cache hit time (in clock cycles)
	unsigned cache_miss_penalty;		// cache miss penalty (in clock cycles)	
	unsigned cache_address_width;  		// number of bits in memory address

	// #sets = #blocks/assoc = size / (block-sz * assoc)
	unsigned set_count; 

	// bit counts
	unsigned idx_bits;
	unsigned offset_bits;
	unsigned tag_bits;

	// bit masks
	unsigned long long idx_mask;
	unsigned long long offset_mask;
	unsigned long long tag_mask;

	// 2D array to store cache information
	cache_block_t **cache_s;

	/* number of memory accesses processed */
	unsigned number_memory_accesses;
	unsigned number_reads;
	unsigned number_read_misses;
	unsigned number_writes;
	unsigned number_write_misses;
	unsigned number_evictions;
	unsigned number_mem_writes;

	unsigned write_thrus;
	unsigned write_backs;
	unsigned write_allocates;
	unsigned no_write_allocates;

	/* trace file input stream */	
	ifstream stream;


public:

	/* Instantiates the cache simulator */
	cache(
		unsigned size, 					// cache size (in bytes)
        unsigned associativity,     	// cache associativity (fully-associative caches not considered)
	    unsigned line_size,         	// cache block size (in bytes)
	    write_policy_t wr_hit_policy,  	// write-back or write-through
	    write_policy_t wr_miss_policy, 	// write-allocate or no-write-allocate
	    unsigned hit_time,				// cache hit time (in clock cycles)
	    unsigned miss_penalty,			// cache miss penalty (in clock cycles)	
	    unsigned address_width          // number of bits in memory address
	);	
	
	// de-allocates the cache simulator
	~cache();

	// loads the trace file (with name "filename") so that it can be used by the "run" function  
	void load_trace(const char *filename);

	// processes "num_memory_accesses" memory accesses (i.e., entries) from the input trace 
	// if "num_memory_accesses=0" (default), then it processes the trace to completion 
	void run(unsigned num_memory_accesses=0);
	
	// processes a read operation and returns hit/miss
	access_type_t read(address_t address);
	
	// processes a write operation and returns hit/miss
	access_type_t write(address_t address);

	// returns the next block to be evicted from the cache
	unsigned evict(unsigned set);
	
	// prints the cache configuration
	void print_configuration();
	
	// prints the execution statistics
	void print_statistics();

	//prints the metadata information (including "dirty" but, when applicable) for all valid cache entries  
	void print_tag_array();

	//gets the write policy
	string get_policy(bool type);

	//get average access time
	double get_average_access_time();

	//get number of memory writes
	unsigned num_of_mem_writes();


};

#endif /*CACHE_H_*/
