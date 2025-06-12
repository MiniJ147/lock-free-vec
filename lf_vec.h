#ifndef LF_VEC_H
#define LF_VEC_H
#include <cassert>
#include <cmath>
#include <atomic>
#include <iostream>
#include <thread>
#include "descriptors.h"
#include "mem_pool.h"

// warning do not change this as it effects our alloc_bucket shifting
// this change was because we were having indexing issues earlier in the implementation
// so to be on the safe side do NOT change
#define FIRST_BUCKET_SIZE 8 // as stated in 3.3 operations

// setting our max L1 size because it grows exponentially with powers of 2
// ie) our max size is = 2^1 + 2^2 + 2^3 + ... + 2^(MAX_L1_SIZE)
#define VEC_L1_MAX_SIZE 32

// max threads we are going to use on our vector
// this is necessary for the pool since it needs to know how much memory to allocate up front
// going under the number ins't a problem but spawning over the MAX_THREADS will cause weird memory issues
// due to over writes and conflicts which shouldn't happen (or the program will get stuck in infinite loop)
// put simply don't go over this number
// #define MAX_THREADS 32
int MAX_THREADS = 32;

// this should equla MAX_THREADS
// but you can set it to 1 for orginal version
// #define MAX_POOLS 1
int MAX_POOLS = 1;

// as stated in comments further down in the code
#define POOL_SIZE 2*MAX_THREADS+1

thread_local int thread_id = -1;
thread_local int thread_pool = -1;

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

// alternative (untested) to __bit_width as some version don't allow it
// int highest_bit(int x){
//     for(int i=31; i>0; i--){
//         if(x & (0b1<<i))
//             return i;
//     }
//     return 0;
// }

namespace lockfree {
// contains the logic and functions necessary to complete vector operations
// what the user will use at an abstract level
template <typename T>
class Vector {
private:
    // array of atomic pointers, pointing to an array of atomic references of T 
    std::atomic<std::atomic<T>*> memory[VEC_L1_MAX_SIZE]; 

    std::atomic<Descriptor<T>*> _descriptor; // benchmarking with leaks

    std::atomic<mem::Node<T>*> descriptor;

    // our memory pool
    // 
    // why is our memory complexity O(2*MAX_THREADS+1)?
    // 
    // due to the way we reference nodes each thread can have a max of 2 references
    // 1 to the Vector descriptor and one to its personal local copy 
    // 
    // since threads can be delayed arbitrarily these references can lag behind successful CAS operations
    // but since they will only have a max of 2 references the worst case is
    // that each thread has one reference to an delayed descriptor and one to its local copy
    //
    // We then have +1 for the Vector Descriptor (which is its own reference) 
    // I think the +1 is unnecessary because of the progress gunarate of at least one thread succeeding
    // but I didn't prove this hard so I left it in
    // mem::Pool<T> pool = mem::Pool<T>(2*(MAX_THREADS)+1);
    
    // mega pool used for bench marking
    // id is the given thread
    mem::Pool<T>* pools = new mem::Pool<T>[MAX_THREADS];
    

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
        
        // printf("at(%d): bucket: %d | new_idx: %d\n",idx,hibit-highest_bit(FIRST_BUCKET_SIZE),new_idx);
        return &this->memory[hibit - highest_bit(FIRST_BUCKET_SIZE)][new_idx];
    }

    // allocates a new bucket(index) 
    // new_bucket_size = FIRST_BUCKET_SIZE^(bucket+1)
    void alloc_bucket(int bucket){
        // int bucket_size = pow(FIRST_BUCKET_SIZE,bucket+1);
        int bucket_size = 0b1 << (bucket+3); // we add 3 to it because we want to start our bucket off at 8

        std::atomic<T>* bucket_new = new std::atomic<T>[bucket_size]; // alloc new bucket
        std::atomic<T>* bucket_empty = nullptr; // empty bucket

        // attempt to set our bucket
        bool res = this->memory[bucket].compare_exchange_strong(bucket_empty,bucket_new);
        
        // someone has already alloced this bucket
        if(!res){ 
            delete[] bucket_new;
            return;
        }
        // std::cout<<"alloced new bucket size "<<bucket_size<<" for bucket "<<bucket<<std::endl;
    }

    mem::Node<T>* fetch_descriptor() {
        while (true) {
            // fetch local copy
            mem::Node<T>* node = this->descriptor.load(std::memory_order_acquire);

            // attempt to insert our reference on the block
            node = this->pools[node->pool_id].alloc(node->id);

            // check to make sure descriptor didn't change
            // if it did then our reference will be invalid
            if (node == this->descriptor.load(std::memory_order_acquire)) {
                return node;
            }

            // Someone else swapped it — roll back our reference
            pools[node->pool_id].release(node->id);
        }
    }

    void complete_write(WriteDescriptor<T>* write_op){
        if(write_op != nullptr && !write_op->completed){
            at(write_op->pos)->compare_exchange_strong(write_op->old_val,write_op->new_val);
            write_op->completed = true;
        }
    }

    // why do we drop twice when switching the descriptor
    // the reason is the vectors descriptor counts as it's own reference
    // meaning we need to drop the current Thread and the Vector Descriptor
    // since it moved
    inline void swapped_desc(int pool_id, int desc_id) {
        pools[pool_id].release(desc_id);
        pools[pool_id].release(desc_id); 
    }

    // alloc all of our buckets so we don't have to worry about memory allocation in our algorithms
    void alloc_buckets_bench_mark(int per_thread_operations){
        long int max_size = per_thread_operations * MAX_THREADS;
        int hibit = highest_bit(max_size);

        for(int bucket_id=1; bucket_id<=hibit; bucket_id++){
            this->alloc_bucket(bucket_id);
        }
    }
public:
    Vector(){
        // defaulting the pointer to NULL for easy alloc_bucket operations
        for(int i=0;i<VEC_L1_MAX_SIZE;i++){
            this->memory[i]=nullptr;
        }

        // allocate our pools
        for(int pool_id=0; pool_id<MAX_POOLS;pool_id++){
            pools[pool_id] = mem::Pool<T>(pool_id,POOL_SIZE);
        }

        // init our first bucket
        alloc_bucket(0);
        this->descriptor.store(pools[0].alloc()); // give thread 0 descriptor reference
        this->_descriptor.store(new Descriptor<T>(nullptr,0));
    }

    // this function is used to inti ourselves for benchmarking purposes
    void init_for_benchmarks(int per_thread_operations){
        alloc_buckets_bench_mark(per_thread_operations);
    }

    void push_back_LEAK(T elem){
        WriteDescriptor<T>* write_op = new WriteDescriptor<T>();
        Descriptor<T>* desc_new = new Descriptor<T>();

        while(true){
            Descriptor<T>* desc_curr = this->_descriptor.load();

            complete_write(desc_curr->write);

            int bucket = highest_bit(desc_curr->size + FIRST_BUCKET_SIZE) - highest_bit(FIRST_BUCKET_SIZE);
            // std::cout<<"pushing on bucket "<<bucket<<std::endl;

            if(this->memory[bucket] == nullptr){
                alloc_bucket(bucket);
            }

            // WriteDescriptor<T>* write_op = new WriteDescriptor<T>(*at(desc_curr->size), elem, desc_curr->size);
            // Descriptor<T>* desc_new = new Descriptor(write_op, desc_curr->size + 1);
            write_op->old_val = *at(desc_curr->size);
            write_op->new_val = elem;
            write_op->pos = desc_curr->size;
            write_op->completed = false;

            desc_new->size = desc_curr->size + 1;
            desc_new->write = write_op;

            if(this->_descriptor.compare_exchange_strong(desc_curr,desc_new)){
                break;
            }
        }   

        complete_write(this->_descriptor.load()->write);
    }

    T pop_back_LEAK(){
        Descriptor<T>* desc_new = new Descriptor<T>(nullptr,0);
        while(true){
            Descriptor<T>* desc_curr = this->_descriptor.load();
            complete_write(desc_curr->write);

            // prevent seg faults idk if this is the best for partical use
            // would have to add errrors or something, but this is for testing
            if(desc_curr->size == 0){                
                return *at(desc_curr->size);
            }

            T res = *at(desc_curr->size - 1);
            // Descriptor<T>* desc_new = new Descriptor<T>(nullptr,desc_curr->size-1);
            desc_new->write = nullptr;
            desc_new->size = desc_curr->size-1;

            if(this->_descriptor.compare_exchange_strong(desc_curr,desc_new)){
                return res;
            }

        }
    }

    // vector functions
    void push_back(T elem){
        mem::Node<T>* thread_node = pools[thread_pool].alloc(); // fetch our block
        while(true){
            mem::Node<T>* curr_node = fetch_descriptor(); // fetch our descriptor (+1 ref)
            Descriptor<T>* desc_curr = &curr_node->desc; // grab desc
 
            complete_write(desc_curr->write);

            // bucket logic
            int bucket = highest_bit(desc_curr->size + FIRST_BUCKET_SIZE) - highest_bit(FIRST_BUCKET_SIZE);
            if(this->memory[bucket] == nullptr){
                alloc_bucket(bucket);
            }

            // new descriptors (local copies)
            WriteDescriptor<T> write_op = WriteDescriptor<T>(*at(desc_curr->size), elem, desc_curr->size);
            Descriptor<T> desc_new = Descriptor(&thread_node->write, desc_curr->size + 1);
            
            // insert our local copies into our memory block
            thread_node->write.replace(write_op); 
            thread_node->desc.replace(desc_new); 
            
            // need to keep old reference to the descriptor since compare_exchange will update curr_node 
            // this is necessary because we need to drop our reference if the descriptor changed
            // however CXS will update our reference which will prevent us from dropping
            // it will also lead to negative references
            // [took an all nighter to figure this out and memory debuggers :( ]
            mem::Node<T>* old_desc_node = curr_node;
            if(this->descriptor.compare_exchange_strong(curr_node,thread_node)){
                // we don't need to add a new reference for the vector descriptor
                // since we will just reuse our reference when fetching the local copy (thread_node)
                swapped_desc(curr_node->pool_id,curr_node->id);
                break;
            }

            // we failed the CAS so we could drop this reference
            pools[old_desc_node->pool_id].release(old_desc_node->id);
        }   

        mem::Node<T>* curr = fetch_descriptor();
        complete_write(&curr->write);
        pools[curr->pool_id].release(curr->id);
    }

    T pop_back(){
        mem::Node<T>* thread_node = pools[thread_pool].alloc(); // fetch our block
        while(true){
            mem::Node<T>* curr_node = fetch_descriptor();
            Descriptor<T>* desc_curr = &curr_node->desc;

            complete_write(desc_curr->write);

            // prevent seg faults idk if this is the best for partical use
            // would have to add errrors or something, but this is for testing
            if(desc_curr->size <= 0){
                pools[curr_node->pool_id].release(curr_node->id);
                pools[thread_node->pool_id].release(thread_node->id);
                return *at(desc_curr->size);
            }

            T res = *at(desc_curr->size - 1);

            Descriptor<T> desc_new = Descriptor<T>(nullptr,desc_curr->size-1);
            thread_node->desc.replace(desc_new);

            mem::Node<T>* old = curr_node;
            if(this->descriptor.compare_exchange_strong(curr_node,thread_node)){
                swapped_desc(curr_node->pool_id,curr_node->id);
                return res;
            }

            pools[old->pool_id].release(old->id);
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
    size_t size(){
        mem::Node<T>* block = fetch_descriptor();
        size_t size = block->desc.size;

        // pending...
        if(block->desc.write_op_pending()){
            pools[block->pool_id].release(block->id);
            return size-1;
        }

        pools[block->pool_id].release(block->id);
        return size;
    } 

    void set_id(int id){
        thread_id = id;
        thread_pool = thread_id % MAX_POOLS;
    }
};
};

#endif
