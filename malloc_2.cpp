#include <unistd.h>
#include <string.h>

const int ALLOCATE_MAX_SIZE = 100000000;


struct Mallocmetadata{
    size_t  size;
    bool is_free;
    Mallocmetadata* next;
    Mallocmetadata* prev;
    void* p;
};


//global pointer to the list that point to the first metadata structure.
Mallocmetadata* firstMetadate = nullptr;


static void* my_alloc(Mallocmetadata** node, Mallocmetadata* next, Mallocmetadata* prev, size_t size) {
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

    return  allocBlock; //TODO: Check this section returnig a pointer to begining of block including metadata

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



