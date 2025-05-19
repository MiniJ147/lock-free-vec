#ifndef LF_VEC_H
#define LF_VEC_H
#include <cassert>
#include <cmath>
#include <atomic>
#include <iostream>
#include "descriptors.h"

#define FIRST_BUCKET_SIZE 8 // as stated in 3.3 operations

// setting 16 as our max becuase our vector grows exponentially with FIRST_BUCKET_SIZE
// here our max elements would be the sum of FIRST_BUCKET_SIZE^(1...16)
// or in other words FIRST_BUCKET_SIZE^(1....VEC_L1_MAX_SIZE)
// also we are limited by the number of bits used for indexing
#define VEC_L1_MAX_SIZE 16

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
    // array of atomic pointers, pointing to an array of atomic references of T 
    std::atomic<std::atomic<T>*> memory[VEC_L1_MAX_SIZE]; 

    std::atomic<Descriptor<T>*> descriptor;

    // indexes into our array at the specfic spot we need with clever bitwise operations
    // simply put, our bucket size grows in powers of 8 (assuming 8 is the first bucket size)
    // we can then use the MSB to mark the number of buckets we currently have in the array 
    // with that we can mask to find the specfic element in that array section for that bucket
    std::atomic<T>* at(int idx){
        int pos = idx + FIRST_BUCKET_SIZE; // get our requested position
        int hibit = highest_bit(pos); 

        // translate this pos into an index for our array section in the bucket 
        // (trimming the MSB)
        int new_idx = pos ^ (1<<hibit); // 1<<(hibit) = 2^(hibit) assuming hibit >= 1
        
        return &this->memory[hibit - highest_bit(FIRST_BUCKET_SIZE)][new_idx];
    }

    // allocates a new bucket(index) 
    // new_bucket_size = FIRST_BUCKET_SIZE^(bucket+1)
    void alloc_bucket(int bucket){
        int bucket_size = pow(FIRST_BUCKET_SIZE,bucket+1);

        std::atomic<T>* bucket_new = new std::atomic<T>[bucket_size]; // alloc new bucket
        std::atomic<T>* bucket_empty = nullptr; // empty bucket

        // attempt to set our bucket
        bool res = this->memory[bucket].compare_exchange_strong(bucket_empty,bucket_new);
        
        // someone has already alloced this bucket
        if(!res){ 
            delete[] bucket_new;
        }
        std::cout<<"alloced new bucket size "<<bucket_size<<" for bucket "<<bucket<<std::endl;
    }
public:
    Vector(){
        // defaulting the pointer to NULL for easy alloc_bucket operations
        for(int i=0;i<VEC_L1_MAX_SIZE;i++){
            this->memory[i]=nullptr;
        }

        // init our first bucket
        alloc_bucket(0);
        this->descriptor.store(new Descriptor<T>(nullptr,0,0));
    }

    // vector functions
    void resize(size_t size);

    // [TODO]: add proper memory managment
    void push_back(T elem){
        while(true){
            Descriptor<T>* desc_curr = this->descriptor.load();
            complete_write(desc_curr->write);

            int bucket = highest_bit(desc_curr->size + FIRST_BUCKET_SIZE) - highest_bit(FIRST_BUCKET_SIZE);
            std::cout<<"pushing on bucket "<<bucket<<std::endl;

            if(this->memory[bucket] == nullptr){
                alloc_bucket(bucket);
            }

            WriteDescriptor<T>* write_op = new WriteDescriptor<T>(*at(desc_curr->size), elem, desc_curr->size);
            Descriptor<T>* desc_new = new Descriptor(write_op, desc_curr->size + 1, 0);

            if(this->descriptor.compare_exchange_strong(desc_curr,desc_new)){
                break;
            }

            delete write_op;
            delete desc_new;
        }   

        complete_write(this->descriptor.load()->write);
    }

    // [TODO]: add memory managment
    T pop_back(){
        while(true){
            Descriptor<T>* desc_curr = this->descriptor.load();
            complete_write(desc_curr->write);

            T res = *at(desc_curr->size - 1);
            Descriptor<T>* desc_new = new Descriptor<T>(nullptr,desc_curr->size-1,0);

            if(this->descriptor.compare_exchange_strong(desc_curr,desc_new)){
                return res;
            }
        }
    }

    // random accesses
    void write_at(size_t idx, T val){
        at(idx)->store(val);
    }

    T read_at(size_t idx){
        return at(idx)->load();
    }

    // other
    void complete_write(WriteDescriptor<T>* write_op){
        if(write_op != nullptr && !write_op->completed){
            at(write_op->pos)->compare_exchange_strong(write_op->old_val,write_op->new_val);
            write_op->completed = true;
        }
    }


    size_t size(){
        Descriptor<T>* desc = this->descriptor.load();
        size_t size = desc->size;

        // pending...
        if(desc->write_op_pending()){
            return size-1;
        }

        return size;
    }
};
};

#endif
