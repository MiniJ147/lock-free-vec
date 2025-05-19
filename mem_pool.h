#ifndef MEM_POOL_H
#define MEM_POOL_H
#include <atomic>
#include "descriptors.h"

template <typename T>
class Node{
    Descriptor<T> desc; 
    WriteDescriptor<T> write;
};

class Pool{
    int size;
};

#endif
