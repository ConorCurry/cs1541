#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cachesim.h"

/*
Usage:
	./cachesim -I 4096:1:2:R -D 1:4096:2:4:R:B:A -D 2:16384:4:8:L:T:N trace.txt

The -I flag sets instruction cache parameters. The parameter after looks like:
	4096:1:2:R
This means the I-cache will have 4096 blocks, 1 word per block, with 2-way
associativity.

The R means Random block replacement; L for that item would mean LRU. This
replacement scheme is ignored if the associativity == 1.

The -D flag sets data cache parameters. The parameter after looks like:
	1:4096:2:4:R:B:A

The first item is the level and must be 1, 2, or 3.

The second through fourth items are the number of blocks, words per block, and
associativity like for the I-cache. The fifth item is the replacement scheme,
just like for the I-cache.

The sixth item is the write scheme and can be:
	B for write-Back
	T for write-Through

The seventh item is the allocation scheme and can be:
	A for write-Allocate
	N for write-No-allocate

The last argument is the filename of the memory trace to read. This is a text
file where every line is of the form:
	0x00000000 R
A hexadecimal address, followed by a space and then R, W, or I for data read,
data write, or instruction fetch, respectively.
*/

/* These global variables will hold the info needed to set up your caches in
your setup_caches function. Look in cachesim.h for the description of the
CacheInfo struct for docs on what's inside it. Have a look at dump_cache_info
for an example of how to check the members. */
#define CRESET		"\e[0m"
#define CRED		"\e[31m"
#define CGREEN		"\e[32m"

#define RED(X)		CRED X CRESET
#define GREEN(X)	CGREEN X CRESET

static CacheInfo icache_info;
static CacheInfo dcache_info[3];
Cache icache;
Cache dcache[3];
static int ADDR_BITS = 32;
static int BYTE_BITS = 2;

int lg(int power_of_2) {
	int res = 0;
	while (power_of_2 >>= 1) { ++res; }
	return res;
}

void make_cache(Cache *cache, CacheInfo cache_info) {
	int assoc_bits = lg(cache_info.associativity);
	int word_bits = lg(cache_info.words_per_block);
	int row_bits = lg(icache_info.num_blocks) - assoc_bits;
	int tag_bits = ADDR_BITS - BYTE_BITS - word_bits - row_bits;

	//cache->word_shift = BYTE_BITS;
	cache->row_shift = BYTE_BITS + word_bits;
	cache->tag_shift = BYTE_BITS + word_bits + row_bits;

	//cache->word_mask = (1 << word_bits) - 1;
	cache->row_mask = (1 << row_bits) - 1;
	cache->tag_mask = (1 << tag_bits) - 1;

	cache->blocks = (Block **) calloc(cache_info.associativity, sizeof(Block *));
	for (int i = 0; i < cache_info.associativity; i++) {
		cache->blocks[i] = (Block *) calloc(
				cache_info.num_blocks / cache_info.associativity,
				sizeof(Block));
	}
}

void setup_caches()
{
	/* Set up your caches here! */
	/* This call to dump_cache_info is just to show some debugging information
	and you may remove it. */
	//dump_cache_info(); 

	srand(1000);

	make_cache(&icache, icache_info);
	for(int d = 0; d < 3; d++) {
		make_cache(&dcache[d], dcache_info[d]);
	}
}

void handle_ifetch_direct(addr_t address) {
	/* determine: miss or hit? */
	int row_idx = (address >> icache.row_shift) & icache.row_mask;
	int tag = (address >> icache.tag_shift) & icache.tag_mask;
	Block *cur_block = icache.blocks[0] + row_idx;
	if (cur_block->valid) {
		if (cur_block->tag == tag) {
			/* Hit */
			// printf("I_FETCH at %08lx was a " GREEN("HIT\n"), address);
			return;
		} else {
			/* Conflict Miss */
			*cur_block = (Block) {.valid = 1, .tag = tag};
			// printf("I_FETCH at %08lx was a " RED("MISS\n"), address);
			icache.conflict_cnt++;
			icache.lw_cnt += icache_info.words_per_block;
			return;
		}
	} else {
		/* Compulsory Miss */
		*cur_block = (Block) {.valid = 1, .tag = tag};
		// printf("I_FETCH at %08lx was a " RED("MISS\n"), address);
		icache.compulsory_cnt++;
		icache.lw_cnt += icache_info.words_per_block;
		return;
	}

}

void tick() {
	/* Update times for LRU */
	for (int i = 0; i < icache_info.associativity; i++) {
		for (int j = 0; j < icache_info.num_blocks / icache_info.associativity; j++) {
			icache.blocks[i][j].last_used++;
		}
	}
}

Block *getLRU() {
	Block *lru_block = icache.blocks[0];
	for (int i = 0; i < icache_info.associativity; i++) {
		for (int j = 0; j < icache_info.num_blocks / icache_info.associativity; j++) {
			if (icache.blocks[i][j].last_used > lru_block->last_used) {
				lru_block = &icache.blocks[i][j];
			}
		}
	}
	return lru_block;
}
Block *getRandom() {
	int repl_i = random() % icache_info.associativity;
	int repl_j = random() % (icache_info.num_blocks / icache_info.associativity);
	return &icache.blocks[repl_i][repl_j];
}

void handle_ifetch_assoc(addr_t address) {
	int tag = (address >> icache.tag_shift) & icache.tag_mask;
	Block *open_block = NULL;
	tick();
	/* Linear search for matching block */
	/* Will also find an open block if one is available. */
	for (int i = 0; i < icache_info.associativity; i++) {
		for (int j = 0; j < icache_info.num_blocks / icache_info.associativity; j++) {
			if (icache.blocks[i][j].tag == tag) {
				/* Hit */
				icache.blocks[i][j].last_used = 0;
				return;
			}
			if (!open_block && !icache.blocks[i][j].valid) {
				open_block = icache.blocks[i] + j;
			}
		}
	}
	if (open_block) {
		/* Compulsory Miss */
		*open_block = (Block) {.valid = 1, .tag = tag};
		icache.compulsory_cnt++;
		icache.lw_cnt += icache_info.words_per_block;
		return;
	} else if (icache_info.replacement == Replacement_LRU) {
		/* Conflict Miss - Do LRU replacement */
		*getLRU() = (Block) {.valid = 1, .tag = tag};
		icache.conflict_cnt++;
		icache.lw_cnt += icache_info.words_per_block;
		return;
	} else {
		/* Conflict Miss - Do random replacement */
		*getRandom() = (Block) {.valid = 1, .tag = tag};
		icache.conflict_cnt++;
		icache.lw_cnt += icache_info.words_per_block;
		return;
	}
}

void handle_access(AccessType type, addr_t address)
{
	/* This is where all the fun stuff happens! This function is called to
	simulate a memory access. You figure out what type it is, and do all your
	fun simulation stuff from here. */
	switch(type)
	{
		case Access_I_FETCH:
			/* These prints are just for debugging and should be removed. */ 
			/* printf("I_FETCH at %08lx\n", address); */
			if (icache_info.associativity == 1) {
				handle_ifetch_direct(address);
			} else {
				handle_ifetch_assoc(address);
			}
			icache.read_cnt++;
			break;
		case Access_D_READ:
			//printf("D_READ at %08lx\n", address);
			break;
		case Access_D_WRITE:
			//printf("D_WRITE at %08lx\n", address);
			break;
	}
}

void print_statistics()
{
	/* Finally, after all the simulation happens, you have to show what the
	results look like. Do that here.*/
	printf("I-Cache statistics:\n");
	printf("\tNumber of reads performed: \t%d\n", icache.read_cnt);
	printf("\tWords read from memory: \t%d\n", icache.lw_cnt);
	printf("\tRead misses:\n");
	printf("\t  Compulsory misses: \t\t%d\n", icache.compulsory_cnt);
	if (icache_info.associativity == 1)
		printf("\t  Conflict misses: \t\t%d\n", icache.conflict_cnt);
	else
		printf("\t  Capacity misses: \t\t%d\n", icache.conflict_cnt);
	printf("\t  Misses with compulsory: \t%d\n", icache.compulsory_cnt + icache.conflict_cnt);
	printf("\t  Miss rate with compulsory: \t%.2f%%\n", 
			(icache.conflict_cnt + icache.compulsory_cnt)*100./icache.read_cnt);
	printf("\t  Miss rate without compulsory: %.2f%%\n", 
			icache.conflict_cnt*100./icache.read_cnt);  
	if (dcache_info[0].num_blocks) {
		printf("L1 D-Cache statistics:\n");
		printf("\tNumber of reads performed:\t%d\n", dcache[0].read_cnt);
		printf("\tWords read from memory:\t\t%d\n", dcache[0].lw_cnt);
		printf("\tNumber of writes performed:\t%d\n", dcache[0].write_cnt);
		printf("\tWords written to memory:\t%d\n", dcache[0].sw_cnt);
		printf("\tRead misses:\n");
		printf("\t  Compulsory misses:\t\t%d\n", dcache[0].compulsory_cnt);
		if (dcache_info[0].associativity == 1)
			printf("\t  Conflict misses:\t\t%d\n", dcache[0].conflict_cnt);
		else
			printf("\t  Capacity misses:\t\t%d\n", dcache[0].conflict_cnt);
		printf("\t  Misses with compulsory:\t%d\n", dcache[0].compulsory_cnt
							  + dcache[0].conflict_cnt);
		printf("\t  Miss rate with compulsory:\t%.2f%%\n",
				(dcache[0].conflict_cnt + dcache[0].compulsory_cnt)
				* 100./dcache[0].read_cnt);
		printf("\t  Miss rate without compulsory: %.2f%%\n",
				dcache[0].conflict_cnt * 100. / dcache[0].read_cnt);
		printf("\tWrite Misses:\n");
		printf("\t  Compulsory Misses:\t\t%d\n", dcache[0].compulsory_w_cnt);
		printf("\t  Conflict Misses:\t\t%d\n", dcache[0].conflict_w_cnt);
		printf("\t  Misses with compulsory:\t%d\n", dcache[0].compulsory_w_cnt
							  + dcache[0].conflict_w_cnt);
		printf("\t  Miss rate with compulsory:\t%.2f%%\n",
				(dcache[0].conflict_w_cnt + dcache[0].compulsory_w_cnt)
				* 100. / dcache[0].write_cnt);
		printf("\t  Miss rate without compulsory: %.2f%%\n",
				dcache[0].conflict_w_cnt * 100. / dcache[0].write_cnt);
	}
}

/*******************************************************************************
*
*
*
* DO NOT MODIFY ANYTHING BELOW THIS LINE!
*
*
*
*******************************************************************************/

void dump_cache_info()
{
	int i;
	CacheInfo* info;

	printf("Instruction cache:\n");
	printf("\t%d blocks\n", icache_info.num_blocks);
	printf("\t%d word(s) per block\n", icache_info.words_per_block);
	printf("\t%d-way associative\n", icache_info.associativity);

	if(icache_info.associativity > 1)
	{
		printf("\treplacement: %s\n\n",
			icache_info.replacement == Replacement_LRU ? "LRU" : "Random");
	}
	else
		printf("\n");

	for(i = 0; i < 3 && dcache_info[i].num_blocks != 0; i++)
	{
		info = &dcache_info[i];

		printf("Data cache level %d:\n", i);
		printf("\t%d blocks\n", info->num_blocks);
		printf("\t%d word(s) per block\n", info->words_per_block);
		printf("\t%d-way associative\n", info->associativity);

		if(info->associativity > 1)
		{
			printf("\treplacement: %s\n", info->replacement == Replacement_LRU ?
				"LRU" : "Random");
		}

		printf("\twrite scheme: %s\n", info->write_scheme == Write_WRITE_BACK ?
			"write-back" : "write-through");

		printf("\tallocation scheme: %s\n\n",
			info->allocate_scheme == Allocate_ALLOCATE ?
			"write-allocate" : "write-no-allocate");
	}
}

void read_trace_line(FILE* trace)
{
	char line[100];
	addr_t address;
	char type;

	if(fgets(line, sizeof(line), trace) == NULL)
		return;

	if(sscanf(line, "0x%lx %c", &address, &type) < 2)
		return;

	switch(type)
	{
		case 'R': handle_access(Access_D_READ, address);  break;
		case 'W': handle_access(Access_D_WRITE, address); break;
		case 'I': handle_access(Access_I_FETCH, address); break;
		default:
			fprintf(stderr, "Malformed trace file: invalid access type '%c'.\n",
				type);
			exit(1);
			break;
	}
}

static void bad_params(const char* msg)
{
	fprintf(stderr, msg);
	fprintf(stderr, "\n");
	exit(1);
}

#define streq(a, b) (strcmp((a), (b)) == 0)

FILE* parse_arguments(int argc, char** argv)
{
	int i;
	int have_inst = 0;
	int have_data[3] = {};
	FILE* trace = NULL;
	int level;
	int num_blocks;
	int words_per_block;
	int associativity;
	char write_scheme;
	char alloc_scheme;
	char replace_scheme;
	int converted;

	for(i = 1; i < argc; i++)
	{
		if(streq(argv[i], "-I"))
		{
			if(i == (argc - 1))
				bad_params("Expected parameters after -I.");

			if(have_inst)
				bad_params("Duplicate I-cache parameters.");
			have_inst = 1;

			i++;
			converted = sscanf(argv[i], "%d:%d:%d:%c",
				&icache_info.num_blocks,
				&icache_info.words_per_block,
				&icache_info.associativity,
				&replace_scheme);

			if(converted < 4)
				bad_params("Invalid I-cache parameters.");

			if(icache_info.associativity > 1)
			{
				if(replace_scheme == 'R')
					icache_info.replacement = Replacement_RANDOM;
				else if(replace_scheme == 'L')
					icache_info.replacement = Replacement_LRU;
				else
					bad_params("Invalid I-cache replacement scheme.");
			}
		}
		else if(streq(argv[i], "-D"))
		{
			if(i == (argc - 1))
				bad_params("Expected parameters after -D.");

			i++;
			converted = sscanf(argv[i], "%d:%d:%d:%d:%c:%c:%c",
				&level, &num_blocks, &words_per_block, &associativity,
				&replace_scheme, &write_scheme, &alloc_scheme);

			if(converted < 7)
				bad_params("Invalid D-cache parameters.");

			if(level < 1 || level > 3)
				bad_params("Inalid D-cache level.");

			level--;
			if(have_data[level])
				bad_params("Duplicate D-cache level parameters.");

			have_data[level] = 1;

			dcache_info[level].num_blocks = num_blocks;
			dcache_info[level].words_per_block = words_per_block;
			dcache_info[level].associativity = associativity;

			if(associativity > 1)
			{
				if(replace_scheme == 'R')
					dcache_info[level].replacement = Replacement_RANDOM;
				else if(replace_scheme == 'L')
					dcache_info[level].replacement = Replacement_LRU;
				else
					bad_params("Invalid D-cache replacement scheme.");
			}

			if(write_scheme == 'B')
				dcache_info[level].write_scheme = Write_WRITE_BACK;
			else if(write_scheme == 'T')
				dcache_info[level].write_scheme = Write_WRITE_THROUGH;
			else
				bad_params("Invalid D-cache write scheme.");

			if(alloc_scheme == 'A')
				dcache_info[level].allocate_scheme = Allocate_ALLOCATE;
			else if(alloc_scheme == 'N')
				dcache_info[level].allocate_scheme = Allocate_NO_ALLOCATE;
			else
				bad_params("Invalid D-cache allocation scheme.");
		}
		else
		{
			if(i != (argc - 1))
				bad_params("Trace filename should be last argument.");

			break;
		}
	}

	if(!have_inst)
		bad_params("No I-cache parameters specified.");

	if(have_data[1] && !have_data[0])
		bad_params("L2 D-cache specified, but not L1.");

	if(have_data[2] && !have_data[1])
		bad_params("L3 D-cache specified, but not L2.");

	trace = fopen(argv[argc - 1], "r");

	if(trace == NULL)
		bad_params("Could not open trace file.");

	return trace;
}

int main(int argc, char** argv)
{
	FILE* trace = parse_arguments(argc, argv);

	setup_caches();

	while(!feof(trace))
		read_trace_line(trace);

	fclose(trace);

	print_statistics();
	return 0;
}
