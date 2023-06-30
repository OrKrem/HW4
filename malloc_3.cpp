#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

const int ALLOCATE_MAX_SIZE = 100000000;

const int NUMBER_OF_ORDERS = 11;
const int INTIAL_NUMBER_OF_BLOCKS = 32;
const int MAX_ORDER = 10;

uint32_t  Order[NUMBER_OF_ORDERS] = {
    128, // MIN_ORDER
    256, //ORDER_1
    512, //ORDER_2
    1024, //ORDER_3
    2048, //ORDER_4
    4096, //ORDER_5
    8192, //ORDER_6
    16384, //ORDER_7
    32768, //ORDER_8
    65536, //ORDER_9
    131072, //MAX_ORDER
};



struct Mallocmetadata{
    int32_t cookie; // 4 byte
    uint64_t start_address; // 8 byte
    uint64_t  size; // 8 byte 
    bool is_free; // 4 byte
    Mallocmetadata* next; // 8 byte
    Mallocmetadata* prev; // 8 byte
};

// Array to hold free block lists based on order 
Mallocmetadata* free_blocks[NUMBER_OF_ORDERS];

//Global list of mmaps blocks
Mallocmetadata* mmap_list = nullptr;

//global pointer to the list that point to the first metadata structure.
Mallocmetadata* firstMetadate = nullptr;

//Flag if malloc wasn't called
bool is_malloc_first_use = false;

uint32_t global_cookie = 0;

static void _initialize_malloc(){
    if (!is_malloc_first_use){
        return;
    }

    void * current_program_break_address = sbrk(0);

    if (current_program_break_address == (void*) -1) {
        perror("sbrk: failed %s\n", strerror(errno));
        exit(-1);
    }

    firstMetadate = (Mallocmetadata*)((uint64_t)current_program_break_address + ((uint64_t)current_program_break_address & (Order[MAX_ORDER] * INTIAL_NUMBER_OF_BLOCKS))); //current_program_break_address + current_program_break_address % modulo32*128kb
    for(int i=0; i < NUMBER_OF_ORDERS -1 ; i++){
        free_blocks[i] = nullptr;
    }

    global_cookie = random();

    Mallocmetadata* current_block = firstMetadate;
    Mallocmetadata* previous_block = nullptr;
    free_blocks[MAX_ORDER] = firstMetadate;
    for(int i=0; i<INTIAL_NUMBER_OF_BLOCKS; i++){
        current_block->cookie = global_cookie;
        current_block->start_address = (uint64_t) current_block;
        current_block->size = Order[MAX_ORDER];
        current_block->is_free = true;
        current_block->prev = previous_block;
        if(previous_block != nullptr){
            previous_block->next = current_block;
        }
        previous_block = current_block;
        current_block = (Mallocmetadata*) ((void*)current_block + Order[MAX_ORDER]);
    }
    current_block->next = firstMetadate;
    firstMetadate->prev = current_block;
    //First time intilize heap
    //intialize Blocks
    //Intialize free Blocks
}




static void* my_alloc(Mallocmetadata** node, Mallocmetadata* next, Mallocmetadata* prev, size_t size) {
    // condition on size 

    //mmap for big blocks

    //if first allocate

    //search for needed block size

    //search for free block

    //if not get smallest split



    (*node) = (Mallocmetadata*) sbrk(sizeof(Mallocmetadata));
    if ((*node) == (void*) -1){
        return nullptr;
    }

    void* allocatedMemory = sbrk(size);

    if (allocatedMemory == (void*) -1) {
        return nullptr;
    }

    (*node)->size = size;
    (*node)->prev = prev;
    (*node)->next = next;
    (*node)->p = allocatedMemory;
    (*node)->is_free = false;

    if (prev != nullptr)
        prev->next = *node;

    return allocatedMemory;
}
/**
 * Searches for a free block with at least 'size' bytes or allocates a new block using sbrk() if none are found.
 *
 * @param size The size in bytes of the memory block to be allocated.
 * @return On success, returns a pointer to the first byte in the allocated block (excluding meta-data).
 *         On failure, returns NULL under the following conditions:
 *         - If 'size' is 0.
 *         - If 'size' is more than 10^8.
 *         - If sbrk fails to allocate the needed space.
 */
void* smalloc(size_t size){

    //conditions

    //Call my alloc

    if(size == 0 || size > ALLOCATE_MAX_SIZE){
        return nullptr;
    }

    if(!firstMetadate){
        return my_alloc(&firstMetadate, nullptr, nullptr, size);
    }

    Mallocmetadata* curr = firstMetadate;
    Mallocmetadata* prevMD;

    while(curr){
        if(curr->size >= size && curr->is_free){
            curr->is_free = false;
            return curr->p;
        }

        prevMD = curr;
        curr = curr->next;

    }

    void* allocBlock = my_alloc(&curr, nullptr, prevMD, size);

    return  allocBlock;

}

/**
 * Searches for a free block of at least 'num' elements, each 'size' bytes, that are all set to 0
 * or allocates a new block using sbrk() if none are found. In other words, it finds/allocates size * num bytes
 * and sets all bytes to 0.
 *
 * @param num  The number of elements to be allocated.
 * @param size The size in bytes of each element.
 * @return On success, returns a pointer to the first byte in the allocated block.
 *         On failure, returns NULL under the following conditions:
 *         - If either 'size' or 'num' is 0.
 *         - If 'size * num' is more than 108.
 *         - If sbrk fails to allocate the needed space.
 */
void* scalloc(size_t num, size_t size){

    //conditions

    //my alloc

    if(size == 0 || num == 0 || (size * num) > ALLOCATE_MAX_SIZE){
        return nullptr;
    }

    void* allocBlock = smalloc(num * size);

    if(!allocBlock){
        return nullptr;
    }

    memset(allocBlock, 0, size * num);
    return  allocBlock;
}

/**
 * Releases the usage of the block that starts with the pointer 'p'.
 *
 * @param p Pointer to the beginning of the allocated block.
 *          If 'p' is NULL or already released, the function simply returns.
 *          It is presumed that all pointers 'p' truly point to the beginning of an allocated block.
 */
void sfree(void* p){
    //conditions

    //Check how to unify buddy blocks

    if(p == nullptr){
        return;
    }

    Mallocmetadata* curr = firstMetadate;

    while(curr){
        if(curr->p == p){
            curr->is_free = true;
            return;
        }
        curr = curr->next;
    }

    return;
}

/**
 * Resizes the memory block pointed to by 'oldp' to the specified 'size'.
 * If 'size' is smaller or equal to the current block's size, reuses the same block.
 * Otherwise, allocates 'size' bytes for a new space, copies the contents of 'oldp',
 * and frees 'oldp'.
 *
 * @param oldp Pointer to the memory block to be reallocated.
 * @param size The new size in bytes of the memory block.
 * @return On success, returns a pointer to the (newly) allocated space.
 *         If 'oldp' is NULL, allocates space for 'size' bytes and returns a pointer to it.
 *         On failure, returns NULL if 'size' is 0, exceeds the limit (108 bytes),
 *         or if the needed space cannot be allocated.
 *         Note: 'oldp' is not freed if the reallocation fails.
 */
void* srealloc(void* oldp, size_t size){

    //conditions

    //check mmap case

    //How to free buddies?

    
    if(size == 0 || size > ALLOCATE_MAX_SIZE){
        return nullptr;
    }

    if(!oldp){
        return smalloc(size);
    }

    Mallocmetadata* curr = firstMetadate;

    while(curr){
        if(curr->p == oldp){

            //In case size is smaller or equal to the current block's size
            if(curr->size > size){
                return curr->p;
            }

            else{
                auto newlyAllocSpace = smalloc(size);

                if(!newlyAllocSpace){
                    break;
                }

                curr->is_free = true;
                return memcpy(newlyAllocSpace, curr->p, curr->size);
            }
        }

        curr = curr->next;
    }

    return nullptr;
}

/**
 *
 * @return #allocated blocks in the heap that are currently free.
 */
size_t _num_free_blocks(){
    unsigned int numOfFreeBlocks = 0;

    Mallocmetadata* curr = firstMetadate;

    while(curr){
        if(curr->is_free){
            numOfFreeBlocks++;
        }
        curr = curr->next;
    }

    return  numOfFreeBlocks;
}

/**
 *
 * @return #bytes in all allocated blocks in the heap that are currently free, excluding the bytes used by the meta-data structs.
 */
size_t _num_free_bytes(){
    unsigned int numFreeBytes = 0;

    Mallocmetadata* curr = firstMetadate;

    while(curr){
        if(curr->is_free){
            numFreeBytes = numFreeBytes + curr->size;
        }

        curr = curr->next;
    }

    return numFreeBytes;
}

/**
 *
 * @return the overall (free and used) number of allocated blocks in the heap.
 */
size_t _num_allocated_blocks(){
    unsigned int numAllocatedBlocks = 0;

    Mallocmetadata* curr = firstMetadate;

    while(curr){
        numAllocatedBlocks++;

        curr = curr->next;
    }

    return numAllocatedBlocks;
}

/**
 *
 * @return the overall number (free and used) of allocated bytes in the heap, excluding the bytes used by the meta-data structs.
 */
size_t _num_allocated_bytes(){
    unsigned int numAllocatedBytes = 0;

    Mallocmetadata* curr = firstMetadate;

    while(curr){
        numAllocatedBytes = numAllocatedBytes + curr->size;
        curr = curr->next;
    }

    return numAllocatedBytes;
}

/**
 *
 * @return the number of bytes of a single meta-data structure in your system.
 */
size_t _size_meta_data(){
    return sizeof(Mallocmetadata);
}


/**
 *
 * @return the overall number of meta-data bytes currently in the heap..
 */
size_t _num_meta_data_bytes(){


     return _size_meta_data() * _num_allocated_blocks();

}



