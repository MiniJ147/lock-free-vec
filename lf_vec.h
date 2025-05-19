#ifndef LF_VEC_H
#define LF_VEC_H
#include <cassert>
#include <cmath>
#include <atomic>
#include <iostream>
#include <thread>
#include "descriptors.h"
#include "mem_pool.h"

#define FIRST_BUCKET_SIZE 8 // as stated in 3.3 operations

// setting 16 as our max becuase our vector grows exponentially with FIRST_BUCKET_SIZE
// here our max elements would be the sum of FIRST_BUCKET_SIZE^(1...16)
// or in other words FIRST_BUCKET_SIZE^(1....VEC_L1_MAX_SIZE)
// also we are limited by the number of bits used for indexing
#define VEC_L1_MAX_SIZE 32

// used for the pool 
#define MAX_THREADS 32

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

    // std::atomic<Descriptor<T>*> descriptor;
    std::atomic<mem::Node<T>*> descriptor;

    // our memory pool
    mem::Pool<T> pool = mem::Pool<T>(2*(MAX_THREADS)+1);
    

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
        int bucket_size = 0b1 << (bucket+3);

        std::atomic<T>* bucket_new = new std::atomic<T>[bucket_size]; // alloc new bucket
        std::atomic<T>* bucket_empty = nullptr; // empty bucket

        // attempt to set our bucket
        bool res = this->memory[bucket].compare_exchange_strong(bucket_empty,bucket_new);
        
        // someone has already alloced this bucket
        if(!res){ 
            delete[] bucket_new;
            return;
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
        // this->descriptor.store(new Descriptor<T>(nullptr,0,0));
        this->descriptor.store(pool.alloc());
    }
    ~Vector(){
        mem::Node<T>* b = this->descriptor.load();
        std::cout << "ending descriptor: "<<b->id << std::endl;
    }

    // vector functions
    void resize(size_t size);

    mem::Node<T>* fetch_descriptor() {
        while (true) {
            mem::Node<T>* node = this->descriptor.load(std::memory_order_acquire);
            // int old = node->ref.fetch_add(1, std::memory_order_acq_rel);
            node = this->pool.alloc(node->id);

            if (node == this->descriptor.load(std::memory_order_acquire)) {
                return node;
            }

            // Someone else swapped it — roll back our reference
            // node->ref.fetch_sub(1, std::memory_order_acq_rel);
            pool.release(node->id);
    }
}

    // [TODO]: add proper memory managment
    void push_back(T elem){
        mem::Node<T>* thread_node = pool.alloc(); // fetch our block
        // std::cout<<"grabbed node: "<<thread_node->id<<std::endl;
        while(true){
            // std::cout<<"attempting push"<<std::endl;
            mem::Node<T>* curr_node = fetch_descriptor(); // fetch our descriptor
            Descriptor<T>* desc_curr = &curr_node->desc;
 
            // Descriptor<T>* desc_curr = this->descriptor.load();
            // std::cout<<"attempting to complete write"<<std::endl;
            complete_write(desc_curr->write);

            int bucket = highest_bit(desc_curr->size + FIRST_BUCKET_SIZE) - highest_bit(FIRST_BUCKET_SIZE);
            // std::cout<<"pushing on bucket "<<bucket<<std::endl;

            if(this->memory[bucket] == nullptr){
                // std::cout<<desc_curr->size<<" "<<highest_bit(desc_curr->size+FIRST_BUCKET_SIZE)-highest_bit(FIRST_BUCKET_SIZE)<<std::endl;
                alloc_bucket(bucket);
            }

            WriteDescriptor<T> write_op = WriteDescriptor<T>(*at(desc_curr->size), elem, desc_curr->size);
            Descriptor<T> desc_new = Descriptor(&thread_node->write, desc_curr->size + 1, 0);
            
            thread_node->write.replace(write_op); 
            thread_node->desc.replace(desc_new); 


            // WriteDescriptor<T>* write_op = new WriteDescriptor<T>(*at(desc_curr->size), elem, desc_curr->size);
            // Descriptor<T>* desc_new = new Descriptor(write_op, desc_curr->size + 1, 0);
            
            mem::Node<T>* old = curr_node;
            if(this->descriptor.compare_exchange_strong(curr_node,thread_node)){
                swapped_desc(curr_node->id);
                break;
            }
            // std::cout<<"FAILED CAS push_back "<<std::this_thread::get_id()<<"\n";
            pool.release(old->id);
            // pool.release(curr_node->id);
        }   

        // complete_write(this->descriptor.load()->write);
        mem::Node<T>* curr = fetch_descriptor();
        complete_write(&curr->write);
        pool.release(curr->id);
    }

    T pop_back(){
        mem::Node<T>* thread_node = pool.alloc(); // fetch our block
        while(true){
            // Descriptor<T>* desc_curr = this->descriptor.load();
            mem::Node<T>* curr_node = fetch_descriptor();
            Descriptor<T>* desc_curr = &curr_node->desc;

            complete_write(desc_curr->write);

            T res = *at(desc_curr->size - 1);

            Descriptor<T> desc_new = Descriptor<T>(nullptr,desc_curr->size-1,0);
            thread_node->desc.replace(desc_new);

            mem::Node<T>* old = curr_node;
            if(this->descriptor.compare_exchange_strong(curr_node,thread_node)){
                swapped_desc(curr_node->id);
                return res;
            }

            pool.release(old->id);
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
        mem::Node<T>* block = fetch_descriptor();
        size_t size = block->desc.size;

        // pending...
        if(block->desc.write_op_pending()){
            pool.release(block->id);
            return size-1;
        }

        pool.release(block->id);
        return size;
    }

    inline void swapped_desc(int desc_id) {
        pool.release(desc_id);
        pool.release(desc_id); 
    }
};
};

#endif
