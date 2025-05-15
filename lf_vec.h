#ifndef LF_VEC_H
#define LF_VEC_H
#include <cassert>
#include <climits>
#include <iostream>

#define FIRST_BUCKET_SIZE 8 // as stated in 3.3 operations

// NOTE:
// we can replace the HighestBit instruciton with std::bit_width(x) in C++ 20
int highest_bit(int x){
    int res = std::__bit_width(x);

    // result is zero so we don't want to send negative index
    assert(res!=0);

    // return the actual index of the bit (0 based) as BSR documentation states
    // since __bit_width returns the position in (1 based) indexing
    return res - 1;
}

namespace lockfree {
// contains the logic and functions necessary to complete vector operations
// what the user will use at an abstract level
template <typename T>
class Vector {
private:
    // pointer array to valid blocks of memory 
    T** memory; 

    // indexes into our array at the specfic spot we need with clever bitwise operations
    // simply put, our bucket size grows in powers of 8 (assuming 8 is the first bucket size)
    // we can then use the MSB to mark the number of buckets we currently have in the array 
    // with that we can mask to find the specfic element in that array section for that bucket
    T* at(int idx){
        int pos = idx + FIRST_BUCKET_SIZE; // get our requested position
        int hibit = highest_bit(pos); 

        // translate this pos into an index for our array section in the bucket 
        // (trimming the MSB)
        int new_idx = pos ^ (1<<hibit); // 1<<(hibit) = 2^(hibit) assuming hibit >= 1
        
        return &this->memory[hibit - highest_bit(FIRST_BUCKET_SIZE)][new_idx];
    }
public:
    Vector(){
        T** buckets = new T*[1];
        buckets[0] = new T[FIRST_BUCKET_SIZE];

        this->memory = buckets; 
    }

    // vector functions
    void resize(size_t size);
    size_t push_back(T);
    T pop_back();

    // random accesses
    void write_at(size_t idx, T val){
        *at(idx) = val;
    }

    T read_at(size_t idx){
        return *at(idx);
    }

    // other
    size_t size();
};

// WriteDescriptor enforces the semantics of pop_back and push_back operations
// allow for multiple memory locations to be modified in a "single" operation
template <typename T>
class WriteDescriptor{
public:
    T old_val;
    T new_val;
    T* loc;
    bool completed;
};

// Descriptor which holds an optional reference to a write descriptor
// our Descriptor object guarantes the semantics of pop_back and push_back operations
template <typename T>
class Descriptor{
public: 
    WriteDescriptor<T>* write;
    size_t size;
    size_t counter; // reference counter
};

};

#endif
