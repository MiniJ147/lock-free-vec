#ifndef MEM_POOL_H
#define MEM_POOL_H
#include <atomic>
#include <cassert>
#include <stack>
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
    int pool_id;

    Node(): desc(Descriptor<T>(nullptr,0)),write(WriteDescriptor<T>(0,0,0)),ref(0),id(-1),pool_id(-1){};
};

// keeps a record of our objects (a pool of nodes) to pull from
// manages interacting with the memory array which ensures correct behavior 
// up to the user to alloc and release correctly
template <typename T>
class Pool{
private:
    int size;
    Node<T>* mem;
    
    // std::stack<int> free_spots;

public: 
    Pool(){};
    Pool(int pool_id, int size){
        this->size = size;
        this->mem = new Node<T>[size];

        // init our memory array
        for(int i=0;i<size;i++){
            this->mem[i].id = i;
            this->mem[i].pool_id = pool_id;
            // free_spots.push(i);
        }
    }
    ~Pool(){
        // printf("Pool: %d\n",this->mem[0].pool_id);
        // for(int i=0;i<size;i++){ 
        //     std::cout<<"block: "<<i<<" "<<this->mem[i].ref<<" | ";
        // }
        // printf("\n");
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
    // Node<T>* alloc(){
    //     while(1){
    //         int free_node = free_spots.top();
    //         int curr_ref = mem[free_node].ref.fetch_add(1,std::memory_order_acq_rel);
    //
    //         if(curr_ref==0){
    //             free_spots.pop();
    //             return &mem[free_node];
    //         }
    //
    //         mem[free_node].ref.fetch_add(-1,std::memory_order_acq_rel);
    //     }
    // }
    
    // DEBUG
    inline void print_stuff(const char* title){
        printf("pool: %d | %s\n",mem[0].pool_id,title);
        for(int i=0; i<size;i++){
            printf("%d=%d| ",i,mem[i].ref.load());
        }
        printf("\n\n");
    }

    // grab a node (referenced or not) via id from mem pool
    Node<T>* alloc(int id){
        mem[id].ref.fetch_add(1); // fetch from pool
        // print_stuff("al");
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
