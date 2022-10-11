// R. Jesse Chaney
// rchaney@pdx.edu

//Hi this is Nihar
#include "vikalloc.h"

#define BLOCK_SIZE (sizeof(mem_block_t))
#define BLOCK_DATA(__curr) (((void *) __curr) + (BLOCK_SIZE))
#define DATA_BLOCK(__data) ((mem_block_t *) (__data - BLOCK_SIZE))

#define IS_FREE(__curr) ((__curr -> size) == 0)

#define PTR "0x%07lx"
#define PTR_T PTR "\t"

static mem_block_t *block_list_head = NULL;
static mem_block_t *block_list_tail = NULL;
static void *low_water_mark = NULL;
static void *high_water_mark = NULL;
// only used in next-fit algorithm
static mem_block_t *prev_fit = NULL;

static uint8_t isVerbose = FALSE;
static vikalloc_fit_algorithm_t fit_algorithm = FIRST_FIT;
static FILE *vikalloc_log_stream = NULL;

static void init_streams(void) __attribute__((constructor));

static size_t min_sbrk_size = MIN_SBRK_SIZE;

static void 
init_streams(void)
{
    vikalloc_log_stream = stderr;
}

size_t
vikalloc_set_min(size_t size)
{
    if (0 == size) {
        // just return the current value
        return min_sbrk_size;
    }
    if (size < (BLOCK_SIZE + BLOCK_SIZE)) {
        // In the event that it is set to something silly small.
        size = MAX(BLOCK_SIZE + BLOCK_SIZE, SILLY_SBRK_SIZE);
    }
    min_sbrk_size = size;

    return min_sbrk_size;
}

void 
vikalloc_set_algorithm(vikalloc_fit_algorithm_t algorithm)
{
    fit_algorithm = algorithm;
    if (isVerbose) {
        switch (algorithm) {
        case FIRST_FIT:
            fprintf(vikalloc_log_stream, "** First fit selected\n");
            break;
        case BEST_FIT:
            fprintf(vikalloc_log_stream, "** Best fit selected\n");
            break;
        case WORST_FIT:
            fprintf(vikalloc_log_stream, "** Worst fit selected\n");
            break;
        case NEXT_FIT:
            fprintf(vikalloc_log_stream, "** Next fit selected\n");
            break;
        default:
            fprintf(vikalloc_log_stream, "** Algorithm not recognized %d\n"
                    , algorithm);
            fit_algorithm = FIRST_FIT;
            break;
        }
    }
}

void
vikalloc_set_verbose(uint8_t verbosity)
{
    isVerbose = verbosity;
    if (isVerbose) {
        fprintf(vikalloc_log_stream, "Verbose enabled\n");
    }
}

void
vikalloc_set_log(FILE *stream)
{
    vikalloc_log_stream = stream;
}

//version of Malloc: step 1 is complete
void *
vikalloc(size_t size)
{
	mem_block_t *curr = NULL;
	mem_block_t *check_free = NULL;

	
	int sbrk_size = (((size + BLOCK_SIZE)/min_sbrk_size) + 1) * min_sbrk_size; //calculate size of sbrk block 

	//declared here to prevent error messages
	mem_block_t * add_split = NULL;	
	mem_block_t * hold_prev = NULL;

//____________________________________________________________________________________________________________________________________________________
	if(size == 0) return NULL;	

//____________________________________________________________________________________________________________________________________________________

//Case 1: HEAP DOESNT EXIST
	if(block_list_head == NULL){
		curr = sbrk(sbrk_size);
		curr -> size = size;
		curr -> capacity = sbrk_size - BLOCK_SIZE;	

		low_water_mark = curr;
		high_water_mark = low_water_mark + sbrk_size;//can do this because low water mark is void ptr
		//will need to change size and capacity later
		block_list_head = curr;
		block_list_tail = block_list_head;
		
		block_list_tail -> next = NULL;
		block_list_tail -> prev = NULL;
	}

//____________________________________________________________________________________________________________________________________________________
//Case 2, 3, etc: HEAP DOES EXIST 
	else{
		
		check_free = block_list_head;

		//This loops through all the blocks to check if any block is free 
		while (check_free != NULL){

			long unsigned int excess_capacity = (check_free -> capacity) - (check_free -> size);

//Case 2: Finding an EMPTY BLOCK: When check_free is empty and capacity is large enough, store data there			
			//Reusing Empty Block: Chaney
			if(IS_FREE(check_free) && check_free -> capacity >= size){
				check_free -> size = size;
				return BLOCK_DATA(check_free);
			}

//______________THIS IS WHERE THE ERROR IS _______________________
//Case 3: split block
			if(excess_capacity >= (size + BLOCK_SIZE)){
				
				int prev_capacity = check_free -> capacity;
				
				check_free -> capacity = check_free -> size;

				//MUST CHECK IF CORRECT: cast check_free to void ptr to do arithmatic at byte level
				// same as below: add_split = ((void *) check_free) + BLOCK_SIZE + check_free->capacity;

				add_split = BLOCK_DATA(check_free) + check_free -> size;

				add_split -> size = size;
				add_split -> capacity = prev_capacity - BLOCK_SIZE - check_free->size;//calculate new capacity


				//PTR wrangling 
				add_split -> prev = check_free;

				if(check_free -> next){
					check_free -> next -> prev = add_split; 
					add_split -> next = check_free -> next;
				}
				//tail case
 				else{
					block_list_tail = add_split;
					add_split -> next = NULL;
				}
				
				check_free -> next = add_split; 
				
				/*
				curr = add_split; //set curr = add_split 

				if (block_list_tail == check_free) block_list_tail = curr;//updating tail
				break;
				*/
				
				return BLOCK_DATA(add_split);
			} 
				
/* Copy of Chaney
	//Case 2b: Set pointer to free block between blocks
		
			if((check_free -> size == 0) && (check_free -> capacity >= size)){ 
				check_free -> size = size;

				curr = check_free;

				if (block_list_tail == check_free -> prev) block_list_tail = curr;//updating tail
				break; //break out of Loop if free Blocks between blocks
			}
*/
			
			check_free = check_free -> next;		
	
		}
/*DELETE
		//If the block is free set curr = check_free and set size of check_free: setting current to check_free will return the address to the free block
		if(check_free != NULL){
		}
*/


	//Case 3: No free blocks: MUST ADD BLOCK

		//if there is no free block, you must call sbrk again
		if(check_free == NULL){ 
			//Adding more memory to the HEAP and a new block to the linked list
			curr = sbrk(sbrk_size);
			high_water_mark += sbrk_size;//high_water_mark is a void ptr

			curr -> size = size;
			curr -> capacity = sbrk_size - BLOCK_SIZE;	

			block_list_tail -> next = curr;
			
			hold_prev = block_list_tail;
			block_list_tail = block_list_tail -> next;

			block_list_tail -> prev = hold_prev;
			block_list_tail -> next = NULL;
		}
		
		
	}

		

	if (isVerbose) {
	fprintf(vikalloc_log_stream, ">> %d: %s entry: size = %lu\n"
		, __LINE__, __FUNCTION__, size);
    }

    return BLOCK_DATA(curr);
}


//Must fix the tail_ptr
void vikfree(void *ptr)
{

	//Curr is the header of the Object stored in ptr
	mem_block_t *curr = NULL; //can do this bc void ptr is being used

	mem_block_t * upper_block = NULL;
	mem_block_t * lower_block = NULL;
	

	if(ptr == NULL) return;
	//set data in heap block as free by setting size to 0		
	curr = ptr - BLOCK_SIZE; //can do this bc void ptr is being used
	//same as: mem_block_t * curr = DATA_BLOCK(ptr)	


//1. Checks if curr is already free 	
	if(IS_FREE(curr)){
		
		if(isVerbose) fprintf(vikalloc_log_stream, "Block is already free: ptr = " PTR "\n", (long) (ptr - low_water_mark));

		return;
	
	}
	
//2. Sets size of curr to 0 to free curr 
	//freeing curr
	curr -> size = 0;
	



	//This portion of VIKFREE is used to implement coalescing

	//Coalesce UP
	upper_block = curr -> next;
	if(upper_block && IS_FREE(upper_block)){
		
		if(upper_block == block_list_tail) block_list_tail = curr;

		curr -> capacity += BLOCK_SIZE + (upper_block -> capacity);
		curr -> next = upper_block -> next;

		if(upper_block -> next != NULL) upper_block->next-> prev = curr;
		
		/* Not necessary
		upper_block -> next = NULL;
		upper_block -> prev = NULL;	
		*/
		
	} 
///*	
	//do a recursive call if the lower block is already free to coalesce
	lower_block = curr -> prev;
	if(lower_block && IS_FREE(lower_block)){
		lower_block -> size = 1;//this prevents a double free

		vikfree(BLOCK_DATA(lower_block));//recursive call
	}
//*/	
/*	
	//Coalesce DOWN
	upper_block = curr;
	lower_block = curr -> prev;
	if(lower_block && lower_block -> size == 0){
		
		if(upper_block == block_list_tail) block_list_tail = lower_block;

		lower_block -> capacity += BLOCK_SIZE + (upper_block -> capacity);
		lower_block -> next = upper_block -> next;

		if(upper_block -> next != NULL) upper_block->next-> prev = lower_block;

		upper_block -> next = NULL;
		upper_block -> prev = NULL;	
	}
*/
		
	if (isVerbose) fprintf(vikalloc_log_stream, ">> %d: %s entry\n", __LINE__, __FUNCTION__);

    	return;
}


/*
void coalesce_func
*/




//DONE: Resetting the heap
void 
vikalloc_reset(void)
{
	//reset heap using brk, set low water and high water marks to NULL so heap doesn't exist. Make sure head and tail also no longer point to any memory either. All ptrs have been set to NULL.
	if(low_water_mark != NULL){
		brk(low_water_mark);
		low_water_mark = NULL;
		high_water_mark = NULL;
		
		block_list_head = NULL;
		block_list_tail = NULL; 
	}

	if (isVerbose) {
	fprintf(vikalloc_log_stream, ">> %d: %s entry\n"
		, __LINE__, __FUNCTION__);
	}

	if (low_water_mark != NULL) {
		if (isVerbose) fprintf(vikalloc_log_stream, "*** Resetting all vikalloc space ***\n");
   	}

	return;
}








void *
vikcalloc(size_t nmemb, size_t size)
{
	void *ptr = NULL;

	ptr = vikalloc(nmemb * size);
	
	//setting all uninitialized values in data allocated to 0.	
	ptr = memset(ptr, 0, nmemb * size);


	if (isVerbose) {
	fprintf(vikalloc_log_stream, ">> %d: %s entry\n"
		, __LINE__, __FUNCTION__);
    }

    return ptr;
}

void *
vikrealloc(void *ptr, size_t size)
{
	//Case 1: PTR passed in is NULL: Vikalloc is called
	if (ptr == NULL) return vikalloc(size);

	else{
		mem_block_t * ptr_information = ptr - BLOCK_SIZE;

	//Case 2: new_size is less than or equal to capacity
		if(ptr_information -> capacity >= size) ptr_information -> size = size;
			

	//Case 3: new_size is greater that capacity
		if(ptr_information -> capacity < size){
			
			//allocate memory for new obj
			void * new_ptr = vikalloc(size);

			//copy memory from previous ptr to new_ptr
			mem_block_t * check_size = ptr - BLOCK_SIZE;
			new_ptr = memcpy(new_ptr, ptr, check_size -> size);

			//free memory of previous ptr
			vikfree(ptr);
			ptr = NULL;
			
			ptr = new_ptr;
			new_ptr = NULL;
		}

	}


	if (isVerbose) fprintf(vikalloc_log_stream, ">> %d: %s entry\n", __LINE__, __FUNCTION__);


	return ptr;
}

/*
  void *memcpy(void *dest, const void *src, size_t n);

DESCRIPTION
       The memcpy() function copies n bytes from memory area src to memory area dest.  The memory areas must not overlap.  Use memmove(3) if the memory areas do overlap.
*/


void *
vikstrdup(const char *s)
{
	void *ptr = NULL;

	int array_size = strlen(s) + 1;
	
	ptr = vikalloc(array_size);
	
	ptr = strcpy(ptr, s);

	if (isVerbose) fprintf(vikalloc_log_stream, ">> %d: %s entry\n"	, __LINE__, __FUNCTION__);
	

	return ptr;
}

#include "vikalloc_dump.c"
