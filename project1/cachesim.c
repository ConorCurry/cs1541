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
Cache cache;
static int ADDR_BITS = 32;
static int BYTE_BITS = 2;
static int word_shift;
static int row_shift;
static int tag_shift;
static int word_mask;
static int row_mask;
static int tag_mask;

static int iread_cnt;
static int iloadword_cnt;
static int icompulsory_miss_cnt;
static int iconflict_miss_cnt;

int lg(int power_of_2) {
	int res = 0;
	while (power_of_2 >>= 1) { ++res; }
	return res;
}
void setup_caches()
{
	/* Set up your caches here! */
	/* This call to dump_cache_info is just to show some debugging information
	and you may remove it. */
	dump_cache_info(); 

	int word_bits = lg(icache_info.words_per_block);
	int row_bits = lg(icache_info.num_blocks);
	int tag_bits = ADDR_BITS -BYTE_BITS - word_bits - row_bits;
	printf("WordBits %d, RowBits %d, TagBits %d\n", word_bits, row_bits, tag_bits);
	word_shift = BYTE_BITS;
	row_shift = BYTE_BITS + word_bits; 
	tag_shift = BYTE_BITS + word_bits + row_bits;

	word_mask = (1 << word_bits) - 1;
	row_mask = (1 << row_bits) - 1;
	tag_mask = (1 << tag_bits) - 1;

	cache.blocks = calloc(icache_info.num_blocks, sizeof(Block));
}

void handle_ifetch(addr_t address) {
	/* determine: miss or hit? */
	int row_idx = (address >> row_shift) & row_mask;
	int tag = (address >> tag_shift) & tag_mask;
	Block *cur_block = cache.blocks + row_idx;
	if (cur_block->valid) {
		/* Either Hit or Conflict Miss */
		if (cur_block->tag == tag) {
			/* Hit */
			// printf("I_FETCH at %08lx was a " GREEN("HIT\n"), address);
		} else {
			/* Conflict Miss */
			*cur_block = (Block) {.valid = 1, .tag = tag};
			// printf("I_FETCH at %08lx was a " RED("MISS\n"), address);
			iconflict_miss_cnt++;
			iloadword_cnt += icache_info.words_per_block;
		}
	} else {
		/* Compulsory Miss */
		/* We need to make a new block to put into the cache on a miss */
		*cur_block = (Block) {.valid = 1, .tag = tag};
		// printf("I_FETCH at %08lx was a " RED("MISS\n"), address);
		icompulsory_miss_cnt++;
		iloadword_cnt += icache_info.words_per_block;
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
			handle_ifetch(address);
			iread_cnt++;
			break;
		case Access_D_READ:
			printf("D_READ at %08lx\n", address);
			break;
		case Access_D_WRITE:
			printf("D_WRITE at %08lx\n", address);
			break;
	}
}

void print_statistics()
{
	/* Finally, after all the simulation happens, you have to show what the
	results look like. Do that here.*/
	printf("\nI-Cache statistics:\n");
	printf("\tNumber of reads performed: \t%d\n", iread_cnt);
	printf("\tWords read from memory: \t%d\n", iloadword_cnt);
	printf("\tRead misses:\n");
	printf("\t  Compulsory misses: \t\t%d\n", icompulsory_miss_cnt);
	printf("\t  Conflict misses: \t\t%d\n", iconflict_miss_cnt);
	printf("\t  Miss rate with compulsory: \t%.2f%%\n", icompulsory_miss_cnt*100./iread_cnt);
	printf("\t  Miss rate with conflict: \t%.2f%%\n", iconflict_miss_cnt*100./iread_cnt);  
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
