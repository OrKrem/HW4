#include <unistd.h>

const int ALLOCATE_MAX_SIZE = 100000000;

void* smalloc(size_t size){

    if(size > ALLOCATE_MAX_SIZE || size == 0){
        return nullptr;
    }

    void* prevPB = sbrk(size);

    if(prevPB == (void*)-1){
        return nullptr;
    }

    return prevPB;
}