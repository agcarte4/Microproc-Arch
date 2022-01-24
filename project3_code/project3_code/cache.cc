#include "cache.h"
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <string.h>
#include <iomanip>

using namespace std;

/* Requirements */
//	Works with configurable parameters below
// 	LRU replacement policy
//	add read/write functions to parser
//	add print_configuration
//	add print_statistics (accesses, read/write count, read/write misses, evictions,)


cache::cache(unsigned size, 	
      unsigned associativity,
      unsigned line_size,
      write_policy_t wr_hit_policy,
      write_policy_t wr_miss_policy,
      unsigned hit_time,
      unsigned miss_penalty,
      unsigned address_width
){
	/* edit here */
	cache_size = size;
    cache_associativity = associativity;
	cache_line_size = line_size;
	write_hit_policy = wr_hit_policy;
	write_miss_policy =wr_miss_policy;
	cache_hit_time = hit_time;
	cache_miss_penalty = miss_penalty;
	cache_address_width = address_width;

	// set count
	set_count = cache_size / (cache_line_size * cache_associativity);

	idx_bits = 0;
	offset_bits = 0;
	
	// set the index bits
	unsigned temp = set_count;
	while (temp >>= 1) ++idx_bits;

	// set the offset bits
	temp = cache_line_size;
	while (temp >>= 1) ++offset_bits;

	tag_bits = cache_address_width - idx_bits - offset_bits;
	//cout << "offset bits: " << offset_bits << endl;
	//cout << "index bits: " << idx_bits << endl;
	//cout << "tag bits: " << tag_bits << endl;

	// create masks
	// offset mask
	offset_mask =0;
	for(unsigned i = 0; i < offset_bits; i++){
		offset_mask |= (1 << i);
	}
	//cout << "offset mask: " << std::hex << offset_mask << endl;

	// index mask
	idx_mask = 0;
	for(unsigned i = 0; i < idx_bits; i++){
		idx_mask |= (1 << i);
	}
	idx_mask = (idx_mask << offset_bits);
	//cout << "index mask: " << std::hex << idx_mask << endl;

	// tag mask
	tag_mask = 0;
	for(unsigned i = 0; i < tag_bits; i++){
		tag_mask |= (1 << i);
	}
	tag_mask = (tag_mask << (offset_bits + idx_bits));
	//cout << "tag mask: " << std::hex << tag_mask << endl;


	/*
	tag_mask = (1 << address_width) - 2;
	cout << "tag mask base: " << std::hex << tag_mask << endl;
	tag_mask = tag_mask - idx_mask - offset_mask;
	cout << "tag mask: " << std::hex << tag_mask << endl;*/


	// Create a 2d array for the cache of the provided parameters
	cache_s = new cache_block_t*[cache_associativity];
	for(unsigned i = 0; i < cache_associativity; i++){
		cache_s[i] = new cache_block_t[set_count];
	}

	for( unsigned i = 0; i < cache_associativity; i++){
		for(unsigned j = 0; j < set_count; j++){
			cache_s[i][j].tag = UNDEFINED;
			cache_s[i][j].lru = 0;
			cache_s[i][j].dirty = 0;
		}
	}


	// Clear coutners
	number_memory_accesses = 0;
	number_reads = 0;
	number_read_misses = 0;
	number_writes = 0;
	number_write_misses = 0;
	number_evictions = 0;
	number_mem_writes = 0;

	write_thrus = 0;
	write_backs = 0;
	write_allocates = 0;
	no_write_allocates = 0;

}

void cache::print_configuration(){
	cout << "CACHE CONFIGURATION" << endl;
	cout << "size = " << std::dec << (cache_size >> 10 ) << " KB" << endl;
	cout << "associativity = " << std::dec << cache_associativity << "-way" << endl;
	cout << "cache line size = " << std::dec << cache_line_size << " B" << endl;
	cout << "write hit policy = " << get_policy(1) << endl;
	cout << "write miss policy = " << get_policy(0) << endl;
	cout << "cache hit time = " << std::dec << cache_hit_time << " CLK" << endl;
	cout << "cache miss penalty = " << std::dec << cache_miss_penalty << " CLK" << endl;
	cout << "memory address width = " << std::dec << cache_address_width << " bits" << endl;
}

cache::~cache(){
	/* edit here */

	// free the cache array
	delete(cache_s);
/*
	cache_size = UNDEFINED;
    cache_associativity = UNDEFINED;
	cache_line_size = UNDEFINED;
	write_hit_policy = wr_hit_policy;
	write_miss_policy =wr_miss_policy;
	cache_hit_time = hit_time;
	cache_miss_penalty = miss_penalty;
	cache_address_width = address_width;*/

}

void cache::load_trace(const char *filename){
   stream.open(filename);
}

void cache::run(unsigned num_entries){

   unsigned first_access = number_memory_accesses;
   string line;
   unsigned line_nr=0;

   while (getline(stream,line)){

	line_nr++;
    char *str = const_cast<char*>(line.c_str());
	
    // tokenize the instruction
    char *op = strtok (str," ");
	char *addr = strtok (NULL, " ");
	address_t address = strtoull(addr, NULL, 16);
	//cout << "address: " << std::hex << address << endl;


	access_type_t access;

	if(op[0] == 'r'){ // read
		access = read(address);
		number_reads++;

		if(access == HIT){
			// do something here?

		} else {
			number_read_misses++;


		}
	}else{ // write
		access = write(address);
		number_writes++;

		if(access == HIT){
			// do something here?

		} else {
			number_write_misses++;


		}

	}


	number_memory_accesses++;
	if (num_entries!=0 && (number_memory_accesses-first_access)==num_entries)
		break;
   }
}

void cache::print_statistics(){
	cout << "STATISTICS" << endl;
	cout << "memory accesses = " << std::dec << number_memory_accesses << endl;
	cout << "read = " << std::dec  <<  number_reads << endl;
	cout << "read misses = " << std::dec << number_read_misses << endl;
	cout << "write = " << std::dec << number_writes << endl;
	cout << "write misses = " << std::dec << number_write_misses << endl;
	cout << "evictions = " << std::dec << number_evictions << endl;
	cout << "memory writes = " << std::dec << num_of_mem_writes() << endl;
	cout << "average memory access time = " << get_average_access_time() << endl;
}

access_type_t cache::read(address_t address){
	/* edit here */
	unsigned set;
	unsigned long long tag;

	set = address & idx_mask;
	set >>= (offset_bits);
	tag = (unsigned long long) address & tag_mask;
	tag >>= (idx_bits+offset_bits);

	// check the all cache ways for tag in set
	for(unsigned i = 0; i < cache_associativity; i++){
		if(cache_s[i][set].tag == tag){
			// tag found in cache
			cache_s[i][set].lru = number_memory_accesses;
			return HIT;
		}
	}
	// tag not found in cache, bring from memory to cache
	// first check for free block
	for(unsigned i = 0; i < cache_associativity; i++){
		if(cache_s[i][set].tag == UNDEFINED){
			cache_s[i][set].tag = tag;
			cache_s[i][set].lru = number_memory_accesses;
			cache_s[i][set].dirty = 0;
			return MISS;
		}
	}
	// no free blocks, find way with LRU
	unsigned way = evict(set);

	// evict way/set in cache
	cache_s[way][set].tag = tag;
	cache_s[way][set].dirty = 0;
	cache_s[way][set].lru = number_memory_accesses;

	return MISS;
}

access_type_t cache::write(address_t address){
	unsigned set;
	unsigned long long tag;

	set = address & idx_mask;
	set >>= (offset_bits);
	tag = (unsigned long long) address & tag_mask;
	tag >>=  (idx_bits+offset_bits);

	// check the all cache ways for tag in set
	for(unsigned i = 0; i < cache_associativity; i++){
		if(cache_s[i][set].tag == tag){
			// tag found in cache
			if(write_hit_policy == WRITE_THROUGH){
				// Write-though policy
				cache_s[i][set].lru = number_memory_accesses; // update LRU
				//number_mem_writes++; // write to memory
				write_thrus++;
			}
			else{
				// Write-back policy
				cache_s[i][set].dirty = 1;
				cache_s[i][set].lru = number_memory_accesses; // update LRU
			}
			return HIT;
		}
	}

	// tag not found in cache
	if(write_miss_policy == NO_WRITE_ALLOCATE){
		// miss doesn't affect cache; modify memory
		//number_mem_writes++;
		no_write_allocates++;
		return MISS;
	}
	// The policy is Write-Allocate
	// first check for free block
	for(unsigned i = 0; i < cache_associativity; i++){
		if(cache_s[i][set].tag == UNDEFINED){
			cache_s[i][set].tag = tag;
			cache_s[i][set].lru = number_memory_accesses;
			cache_s[i][set].dirty = 1;
			//number_mem_writes++;
			return MISS;
		}
	}
	// no free blocks, find way with LRU
	unsigned way = evict(set);

	// evict way/set in cache
	cache_s[way][set].tag = tag;
	cache_s[way][set].dirty = 1;
	cache_s[way][set].lru = number_memory_accesses;
	//number_mem_writes++;

	write_allocates++;

	return MISS;
}

void cache::print_tag_array(){
	cout << "TAG ARRAY" << endl;
	/* edit here */
	unsigned i = 0; // block (way)
	unsigned j = 0;	// index (set)

	for(i = 0; i < cache_associativity; i++){
		cout << "BLOCKS " << i << endl;

		///// MAKE two sections based on WRITE-HIT policy (dirty bit)
		if(write_hit_policy == WRITE_BACK){
			//cout << "  index dirty\t  tag" << endl;
			cout << setfill(' ') << setw(7) << "index" << setw(6) << "dirty" << setw(4+tag_bits/4) << "tag" << endl; 
			for(j = 0; j < set_count; j++){
				if((cache_s[i][j].tag + 1 )!= 0){
					cout << setfill(' ') << setw(7) << std::dec << j << setw(6) << std::dec << cache_s[i][j].dirty << std::setw(4) << std::hex <<"0x" << cache_s[i][j].tag << endl;
				}
			}
		}
		else {
			//cout << "  index \t  tag" << endl;
			cout << setfill(' ') << setw(7) << "index" << setw(4+tag_bits/4) << "tag" << endl; 
			for(j = 0; j < set_count; j++){
				if((cache_s[i][j].tag + 1 )!= 0){
					cout << setfill(' ') << setw(7) << std::dec << j << std::setw(4) << std::hex <<"0x" << cache_s[i][j].tag << endl;
				} 
			}
		}
	}
}

unsigned cache::evict(unsigned set){
	number_evictions++;
	//cout << "EVICTION" << endl;

	unsigned lru = cache_s[0][set].lru;
	unsigned way = 0;

	// find LRU
	for(unsigned i = 1; i < cache_associativity; i++){
		// smaller LRU means oldest accessed
		if(cache_s[i][set].lru < lru){
			way = i;
			lru = cache_s[i][set].lru;
		}
	}

	// Update memory if block is dirty
	if(write_hit_policy == WRITE_BACK){	
		if(cache_s[way][set].dirty == 1) write_backs++;//number_mem_writes++;
	}
	
	return way;
}

string cache::get_policy(bool type){
	// get a printable string of the policy type
	if(type){ // type == 1, then this is for the hit policy
		if(write_hit_policy == WRITE_THROUGH) return "write-through";
		else return "write-back";
	} else { // type == 0, for the miss policy 
		if(write_miss_policy == WRITE_ALLOCATE) return "write-allocate";
		else return "no-write-allocate";
	}
}

double cache::get_average_access_time(){
	//int hits = ((number_reads - number_read_misses) + (number_writes - number_write_misses));
	double miss_rate = double (number_read_misses + number_write_misses) / double (number_memory_accesses);

	double avg = miss_rate*cache_miss_penalty + (double)cache_hit_time;
	//(hits*cache_hit_time + misses*cache_miss_penalty)/(number_memory_accesses);
	return avg;
}

unsigned cache::num_of_mem_writes(){
	unsigned count = 0;

	if(write_hit_policy == WRITE_BACK){
		count += write_backs;
	} else {
		count += write_thrus;
	}

	if(write_miss_policy == WRITE_ALLOCATE){
		count += write_allocates;
	} else {
		count += no_write_allocates;
	}

	return count;

}