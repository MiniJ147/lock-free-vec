#ifndef MEM_POOL_H
#define MEM_POOL_H
#include <atomic>
#include <cassert>
#include "descriptors.h"

namespace mem{
template <typename T>
class Node{
public:
    Descriptor<T> desc; 
    WriteDescriptor<T> write;
    std::atomic<int> ref;
    int id;

    Node(): desc(Descriptor<T>(nullptr,0,0)),write(WriteDescriptor<T>(0,0,0)),ref(0),id(-1){};
};

template <typename T>
class Pool{
private:
    int size;
    Node<T>* mem;

public: 
    Pool(int size){
        this->size = size;
        this->mem = new Node<T>[size];

        for(int i=0;i<size;i++){
            this->mem[i].id = i;
        }
    }
    ~Pool(){
        for(int i=0;i<size;i++){
            std::cout<<"block: "<<i<<" "<<this->mem[i].ref<<std::endl;
        }
    }

    Node<T>* alloc(){
        while(1){
            for(int i=0; i<size; i++){
                int curr_ref = mem[i].ref.fetch_add(1,std::memory_order_acq_rel);
                if(curr_ref==0){
                    // std::cout<<"pulling block "<<i<<std::endl;
                    return &mem[i]; 
                }
                mem[i].ref.fetch_add(-1,std::memory_order_acq_rel);
            }
        }
    }
    
    inline void print_stuff(const char* title){
        printf("%s ",title);
        for(int i=0; i<size;i++){
            printf("%d=%d| ",i,mem[i].ref.load());
        }
        printf("\n");
    }

    Node<T>* alloc(int id){
        mem[id].ref.fetch_add(1);
        // print_stuff("al");
        return &mem[id];
    }

    void release(Node<T>* alloc_node){
        for(int i=0; i<size;i++){
            if(alloc_node == &mem[i]){
                mem[i].ref.fetch_add(-1,std::memory_order_acq_rel);
                break;
            }
        }
    }

    void release(int id){
        // std::cout<<"releasing "<<id<<std::endl;
        int old = mem[id].ref.fetch_add(-1,std::memory_order_acq_rel);
        // print_stuff("re");
        // assert(old>0);
    }
};
};

#endif
