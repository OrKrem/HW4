#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <sys/mman.h>

const int ALLOCATE_MAX_SIZE = 100000000;

const int NUMBER_OF_ORDERS = 11;
const int INTIAL_NUMBER_OF_BLOCKS = 32;
const int MAX_ORDER = 10;
const int MIN_ORDER = 0;

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
Mallocmetadata* mmap_list_head = nullptr;

//global pointer to the list that point to the first metadata structure.
Mallocmetadata* firstMetadate = nullptr;

//Flag if malloc wasn't called
bool is_malloc_first_use = false;

uint32_t global_cookie = 0;


/**
 *List functions-remove,insert.
 * I have created a general remove function that can handle cases where mallocmetadata is not the head of the list, even though I am aware that this is the only scenario in which it occurs(for future requirements if needed)
 */
static void remove(Mallocmetadata *mallocmetadata){
    int order;

    for(int i = 0; i < NUMBER_OF_ORDERS; i++){
        if(mallocmetadata->size == Order[i]){
            order = i;
            break;
        }
    }

    Mallocmetadata* curr = free_blocks[order];

    //In case mallocmetadata is the head
    if(curr->start_address == mallocmetadata->start_address){
        if(curr->next != nullptr) {
            curr->next->prev = nullptr;
            free_blocks[order] = curr->next;
            return;
        }

        else{
            free_blocks[order] = nullptr;
            return;
        }
    }

    while(curr){
        if(curr->start_address == mallocmetadata->start_address){

            if(curr->next != nullptr) {
                curr->prev->next = curr->next;
                curr->next->prev = curr->prev;
                return;
            }

            //In case mallocmetadata is the tail
            else{
                curr->prev->next = nullptr;
                return;
            }
        }
        curr = curr->next;
    }

    return;
}


static void insert(Mallocmetadata *mallocmetadata){
    mallocmetadata->next = nullptr;
    mallocmetadata->prev = nullptr;
    int order;

    for(int i = 0; i < NUMBER_OF_ORDERS; i++){
        if(mallocmetadata->size == Order[i]){
            order = i;
            break;
        }
    }

    Mallocmetadata* curr = free_blocks[order];

    if(curr == nullptr){
        free_blocks[order] = mallocmetadata;
        return;
    }

    //In case new block will be the head
    if(curr->start_address > mallocmetadata->start_address){
        mallocmetadata->next = curr;
        free_blocks[order] = mallocmetadata;
        return;
    }

    //In case we have just 1 element in list
    if(curr->next == nullptr){
        curr->next = mallocmetadata;
        mallocmetadata->prev = curr;
        return;
    }


    curr = curr->next;

    while(curr){
        //curr is the tail
        if(curr->next == nullptr){
            if(curr->start_address < mallocmetadata->start_address){
                curr->next = mallocmetadata;
                mallocmetadata->prev = curr;
                return;
            }

            else{
                curr->prev->next = mallocmetadata;
                mallocmetadata->prev = curr->prev;
                mallocmetadata->next = curr;
                return;
            }
        }
        //Standard case-curr with prev and next
        else{
            if(curr->start_address < mallocmetadata->start_address && curr->next->start_address > mallocmetadata->start_address){
                mallocmetadata->next = curr->next;
                mallocmetadata->prev = curr;
                curr->next->prev = mallocmetadata;
                curr->next = mallocmetadata;
                return;
            }
        }
    }

    return;
}

size_t get_list_size(Mallocmetadata* list){
    size_t number_of_elements = 0;
    if(list == nullptr){
        return number_of_elements;
    }
    number_of_elements++;
    Mallocmetadata* current = list;
    do
    {
        number_of_elements++;
        current = current->next;
    } while (current != list); // Completed a circle
    return number_of_elements;
}

/**
 *List functions for mmap_list-remove,insert.
 */
 void insert_mmap(Mallocmetadata* mallocmetadata){
     //In case the list is empty
     if(mmap_list_head == nullptr){
         mmap_list_head = mallocmetadata;
         mallocmetadata->prev = nullptr;
         mallocmetadata->next = nullptr;
         return;
     }

     mallocmetadata->next = mmap_list_head;
     mmap_list_head->prev = mallocmetadata;
     mmap_list_head = mallocmetadata;
     mallocmetadata->prev = nullptr;
     return;
 }

 void remove_mmap(Mallocmetadata* mallocmetadata){
     Mallocmetadata* curr = mmap_list_head;

     //In case mallocmetadata is the head
     if(curr->start_address == mallocmetadata->start_address){
         if(curr->next != nullptr) {
             curr->next->prev = nullptr;
             mmap_list_head = curr->next;
             return;
         }

         else{
             mmap_list_head = nullptr;
             return;
         }
     }

     while(curr){
         if(curr->start_address == mallocmetadata->start_address){

             if(curr->next != nullptr) {
                 curr->prev->next = curr->next;
                 curr->next->prev = curr->prev;
                 return;
             }

             //In case mallocmetadata is the tail
             else{
                 curr->prev->next = nullptr;
                 return;
             }
         }
         curr = curr->next;
     }

     return;
 }




static void _initialize_malloc(){
    if (!is_malloc_first_use){
        return;
    }

    void * current_program_break_address = sbrk(0);

    if (current_program_break_address == (void*) -1) {
        //perror("sbrk: failed %s\n", strerror(errno));
       return;
    }

    uint64_t modulo_to_128kb_32 = ((uint64_t)current_program_break_address & (Order[MAX_ORDER] * INTIAL_NUMBER_OF_BLOCKS-1));
    void * tmp_p = sbrk(INTIAL_NUMBER_OF_BLOCKS*Order[MAX_ORDER] + modulo_to_128kb_32 );//TODO :Dan,redefinition(line 175)
    if (tmp_p == (void*) -1) {
        //perror("sbrk: failed %s\n", strerror(errno));
        exit(-1);
    }

    firstMetadate = (Mallocmetadata*)((uint64_t)current_program_break_address + modulo_to_128kb_32); //current_program_break_address + current_program_break_address % modulo32*128kb


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
        current_block = (Mallocmetadata*) ((char*)current_block + Order[MAX_ORDER]);
    }
    current_block->next = firstMetadate;
    firstMetadate->prev = current_block;

    is_malloc_first_use = false;
    //First time intilize heap
    //intialize Blocks
    //Intialize free Blocks
}

int _get_smallest_fitting_order(uint64_t  size){
    if(size > Order[MAX_ORDER]){ //size is bigger than the Maximum order. Go to mmap()
        return -1;
    }
    else if (size < Order[0]){
        return 0;
    }

    for(int i = MAX_ORDER; i > 0; i--){
        if(size> Order[i-1]){
            return i;
        }
    }
    return -1;
}

Mallocmetadata* _split_block(Mallocmetadata* block){
    if(block == nullptr){
        return nullptr;
    }

    else if(block->size == Order[MIN_ORDER]){
        return nullptr;
    }
    
    //TODO: REMOVE BLOCK from list. Fixed.
    remove(block);
    //TODO: ADD BLOCK to relveant list
    block->size = block->size << 1;
    insert(block);

    Mallocmetadata* second_block = (Mallocmetadata*)(((char*)block) + block->size);//TODO :DAN,why with (void*)?. DAN Fixed 7/1
    second_block->cookie = 0;
    second_block->is_free = true;
    second_block->size = block->size;
    second_block->start_address = (uint64_t)second_block;
    insert(second_block);

    /*
    second_block->next = block->next;//TODO: Or please check me!!!!!!!!!
    block->next = second_block;
    second_block->prev = block;
     */
    return block;
}


static void* my_alloc(size_t size) {
    if(is_malloc_first_use){
        _initialize_malloc();
    }

    int order_size_needed = _get_smallest_fitting_order(size + sizeof(Mallocmetadata));

    if( order_size_needed < 0){ //Need to do mmap
        void* allocatedMemory =  mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0); //TODO:  Rethink the flags
        if (allocatedMemory == (void*) -1){
            return nullptr;
        }
        Mallocmetadata*  metadata = (Mallocmetadata*) allocatedMemory;
        metadata->cookie =global_cookie;
        metadata->is_free = false;
        metadata->size = size;
        metadata->start_address = (uint64_t) allocatedMemory;
        //TODO: ADD to the mmap list with Or's interface;
        insert_mmap(metadata); //TODO: Chsnge to insert mmap. FIXED DAN 1/7
        return (void*) ((char*)allocatedMemory + sizeof(Mallocmetadata));
    }

    if(free_blocks[order_size_needed] != nullptr){ //There is an available block with needed size
        Mallocmetadata*  metadata = free_blocks[order_size_needed];
        metadata->cookie = global_cookie;
        metadata->is_free = false;
        //TODO: REMOVE from freeblocklist
        remove(metadata);
        return (void*) ((char*) metadata + sizeof(Mallocmetadata));//TODO DAN: adin ,why cast to void*? FIXED. DAN 1/7
    }

    int order_size_to_split = -1;
    for (int i = order_size_needed + 1 ; i < NUMBER_OF_ORDERS; i++){
        if(free_blocks[i] != nullptr){
            order_size_to_split = i ;
            break;
        }
    }

    if(order_size_to_split == -1){ // Didn't find big enough block to split.
        return nullptr;
    }

    Mallocmetadata*  block_to_split = free_blocks[order_size_to_split];
    int number_of_order_size_needed = order_size_to_split - order_size_needed;

    for(int i=0; i < number_of_order_size_needed; i++){
        _split_block(block_to_split);
    }

    return (void*) (((char*) block_to_split) + sizeof(Mallocmetadata));

    //if first allocate
    // condition on size 
    //mmap for big blocks
    //search for needed block size
    //search for free block
    //if not get smallest split  
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

    if(size <= 0 || size > ALLOCATE_MAX_SIZE){
        return nullptr;
    }

    return my_alloc(size);

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

    if(size <= 0 || num == 0 || (size * num) > ALLOCATE_MAX_SIZE){
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
    if(p == nullptr){
        return;
    }

    Mallocmetadata* start_of_block = (Mallocmetadata*) ((char*)p - sizeof(Mallocmetadata)); // Get to the MetaData of the block

    if(start_of_block->is_free){ //Block is free
        return;
    }

    else if(start_of_block->size  > Order[MAX_ORDER]){ //MMap 
        remove_mmap(start_of_block);
        munmap(start_of_block,start_of_block->size );
        return;
    }
    Mallocmetadata* current_block  = start_of_block;
    start_of_block->is_free = true;
    Mallocmetadata* buddy = (Mallocmetadata*) (current_block->start_address ^ current_block->size);

    while( current_block->is_free && current_block->size < Order[MAX_ORDER]  && buddy->is_free && buddy->size != current_block->size){
        remove(buddy);
        remove(current_block);
        if (buddy->start_address < current_block->start_address){
            current_block->cookie = 0;
            current_block = buddy;
        }
        else{
            buddy->cookie = 0;
        }
        current_block->size = current_block->size * 2;
        insert(current_block);
        if(current_block->size >= Order[MAX_ORDER]){
            return;
        }
        buddy = (Mallocmetadata*) (current_block->start_address ^ current_block->size);
        Mallocmetadata* tmp = buddy;
        if(buddy->start_address < current_block->start_address){
            current_block = buddy;
            buddy = tmp;
        }   
    }
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

    
    if(size <= 0 || size > ALLOCATE_MAX_SIZE){
        return nullptr;
    }

    if(!oldp){
        return smalloc(size);
    }

    Mallocmetadata* start_of_block = (Mallocmetadata*) ((char*)oldp - sizeof(Mallocmetadata)); // Get to the MetaData of the block

    if( _get_smallest_fitting_order(start_of_block->size) < 0 ){ //MMAP case //TODO: What to do if old_size > new_size
        if(start_of_block->size == size){
            return oldp;
        }
        void* new_p = smalloc(size);
        if(new_p == nullptr){
            return nullptr;
        }
        remove_mmap(start_of_block);
        memmove(new_p,oldp,start_of_block->size);
        munmap(start_of_block,start_of_block->size);
        return new_p;
    }

    if(size < start_of_block->size){//TODO: What to do for MMAP case ?? 
        return oldp;
    }

    uint64_t optional_size_after_merging = start_of_block->size;
    Mallocmetadata*  current_block = start_of_block;
    Mallocmetadata* buddy = (Mallocmetadata*) (current_block->start_address ^ current_block->size);

    while(  (current_block == start_of_block || current_block->is_free) && optional_size_after_merging < size && optional_size_after_merging < Order[MAX_ORDER]  && buddy->is_free && buddy->size != current_block->size){
        if (buddy->start_address < current_block->start_address){
            current_block = buddy;
        }
        optional_size_after_merging = optional_size_after_merging*2;

        if(optional_size_after_merging >= Order[MAX_ORDER]){
            break;
        }
        buddy = (Mallocmetadata*) (current_block->start_address ^ optional_size_after_merging);
        Mallocmetadata* tmp = buddy;
        if(buddy->start_address < current_block->start_address){
            current_block = buddy;
            buddy = tmp;
        }
    }

    if(optional_size_after_merging < size){ // Unable to merge blocks to get desired block
        void* new_p = smalloc(size);
        if(new_p == nullptr){
            return nullptr;
        }
        memmove(new_p,oldp,start_of_block->size);
        sfree(oldp);
        return new_p;
    }

    current_block = start_of_block;
    int order_number_to_merge = _get_smallest_fitting_order(optional_size_after_merging);
    int starting_order_number = _get_smallest_fitting_order(start_of_block->size);
    remove(start_of_block);
    for(int i = starting_order_number; i<order_number_to_merge; i++ ){
        buddy = (Mallocmetadata*) (current_block->start_address ^ current_block->size);
        remove(buddy);
        buddy->cookie = 0;
        if (buddy->start_address < current_block->start_address){
            current_block = buddy;
        }
        current_block->size = current_block->size * 2;
    }
    current_block->cookie = global_cookie;
    current_block->is_free = false;
    memmove((void *)(current_block->start_address + sizeof(Mallocmetadata)),oldp,start_of_block->size);
    return (void*) ((char*)current_block + sizeof(Mallocmetadata));
} 





/**
 *
 * @return #allocated blocks in the heap that are currently free.
 */
size_t _num_free_blocks(){
    size_t numOfFreeBlocks = 0;
    for(int i=0; i < NUMBER_OF_ORDERS; i++){
        numOfFreeBlocks += get_list_size(free_blocks[i]);
    }
    return numOfFreeBlocks;
}

/**
 *
 * @return #bytes in all allocated blocks in the heap that are currently free, excluding the bytes used by the meta-data structs.
 */
size_t _num_free_bytes(){
    size_t numFreeBytes = 0;
    for(int i=0; i < NUMBER_OF_ORDERS; i++){
        numFreeBytes += get_list_size(free_blocks[i]) * (Order[i]-sizeof(Mallocmetadata));
    }
    return numFreeBytes;
}

/**
 *
 * @return the overall (free and used) number of allocated blocks in the heap.
 */
size_t _num_allocated_blocks(){
    size_t number_of_mmap_elements = get_list_size(mmap_list_head);
    size_t numAllocatedBlocks = 0;

    Mallocmetadata* curr = firstMetadate;

    while((uint64_t)curr != (firstMetadate->start_address + INTIAL_NUMBER_OF_BLOCKS*Order[MAX_ORDER])){
        numAllocatedBlocks++;
        curr = (Mallocmetadata*)(curr->start_address + curr->size);
    }
    return (numAllocatedBlocks+number_of_mmap_elements);
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

/**
 *
 * @return the overall number (free and used) of allocated bytes in the heap, excluding the bytes used by the meta-data structs.
 */
size_t _num_allocated_bytes(){
    size_t number_of_mmap_bytes = 0;
    Mallocmetadata* curr = mmap_list_head;
    if(curr != nullptr){
    do
    {
        number_of_mmap_bytes += (curr->size);
        curr = curr->next;
    } while (curr != mmap_list_head); 
    }

    return (INTIAL_NUMBER_OF_BLOCKS*Order[MAX_ORDER] + number_of_mmap_bytes - _num_meta_data_bytes()); // HEAP_SIZE + MMAP_SIZE - METADATA

    

}