#ifndef MEM_POOL_H
#define MEM_POOL_H
#include <atomic>
#include <cassert>
#include "descriptors.h"

namespace mem{

// Node is our object which our memory pool will contain
template <typename T>
class Node{
public:
    Descriptor<T> desc; 
    WriteDescriptor<T> write;
    std::atomic<int> ref; // reference counter
    int id; // pos in the memory array

    Node(): desc(Descriptor<T>(nullptr,0)),write(WriteDescriptor<T>(0,0,0)),ref(0),id(-1){};
};

// keeps a record of our objects (a pool of nodes) to pull from
// manages interacting with the memory array which ensures correct behavior 
// up to the user to alloc and release correctly
template <typename T>
class Pool{
private:
    int size;
    Node<T>* mem;

public: 
    Pool(int size){
        this->size = size;
        this->mem = new Node<T>[size];

        // init our memory array
        for(int i=0;i<size;i++){
            this->mem[i].id = i;
        }
    }
    ~Pool(){
        for(int i=0;i<size;i++){
            std::cout<<"block: "<<i<<" "<<this->mem[i].ref<<std::endl;
        }
    }

    // grab a free node (unreferenced) from the memory pool
    Node<T>* alloc(){
        // search for unlimited time since delays could preven't a single sweep find
        while(1){
            for(int i=0; i<size; i++){
                // atempt to grab reference
                int curr_ref = mem[i].ref.fetch_add(1,std::memory_order_acq_rel); 

                // was zero (meaning we successfully got the reference)
                if(curr_ref==0){
                    // std::cout<<"pulling block "<<i<<std::endl;
                    return &mem[i]; 
                }

                // failed to fetch the block so we return our reference
                mem[i].ref.fetch_add(-1,std::memory_order_acq_rel);
            }
        }
    }
    
    // DEBUG
    // inline void print_stuff(const char* title){
    //     printf("%s ",title);
    //     for(int i=0; i<size;i++){
    //         printf("%d=%d| ",i,mem[i].ref.load());
    //     }
    //     printf("\n");
    // }

    // grab a node (referenced or not) via id from mem pool
    Node<T>* alloc(int id){
        mem[id].ref.fetch_add(1); // fetch from pool
        return &mem[id];
    }

    // release a block based off its memory address back to pool
    void release(Node<T>* alloc_node){
        for(int i=0; i<size;i++){
            if(alloc_node == &mem[i]){ 
                mem[i].ref.fetch_add(-1,std::memory_order_acq_rel); //release back to pool
                break;
            }
        }
    }

    // release a block via id from pool
    void release(int id){
        int old = mem[id].ref.fetch_add(-1,std::memory_order_acq_rel); // release back to pool
        assert(old>0); // ensures our reference never went negative
    }
};
};

#endif
