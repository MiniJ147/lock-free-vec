#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include "descriptors.h"
#include "lf_vec.h"

std::atomic<bool> cancel_flag(false);

lockfree::Vector<int> x;
void work(int id){
    int lx = 0;
    int pops = 0;
    while(!cancel_flag){ 
        x.push_back(id);
        if(id & 1 && x.size() > 1){ 
            x.pop_back();
            pops++;
        }
        lx++;
        // std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    // std::this_thread::sleep_for(std::chrono::seconds(id));
    std::cout<<"id: "<<id<<" push: "<<lx<<" pops: "<<pops<<std::endl;
    // std::this_thread::sleep_for(std::chrono::seconds(2));
}

int main(){
    std::cout << "hello world" << std::endl;
    // mem::Pool<int> p(2);
    // mem::Node<int>* x = p.alloc();
    // Descriptor<int>* d = &x->desc;
    // {
    //     Descriptor<int> _new(nullptr,10,0);
    //     d->replace(_new);
    // }
    // std::cout<<x->id<<d->size<<std::endl;
    // p.release(x->id);
    


    std::vector<std::thread> threads;
    for(int i=0; i<MAX_THREADS;i++){
        threads.push_back(std::thread(work,i));
    }

    std::this_thread::sleep_for(std::chrono::seconds(60));
    cancel_flag = true;
    std::cout<<"starting cancel\n";

    for(int i=0; i<MAX_THREADS;i++){
        threads[i].join();
    }

    std::cout<<"sim done...\n";
    std::cout<<"vec size: "<<x.size()<<std::endl;
    int check_ans[MAX_THREADS] = {0};
    for(size_t i=0; i<x.size();i++){
        check_ans[x.read_at(i)]+=1;
    }

    for(int i=0;i<MAX_THREADS;i++){
        std::cout<<i<<": "<<check_ans[i]<<std::endl;
    }
    std::cout<<"\n";


    // std::cout<<x.read_at(0)<<std::endl;
    // x.write_at(0,1);
    // x.complete_write(new lockfree::WriteDescriptor<int>(0, 1, 0));

    // for(int i=10;i<20;i++){
    //     x.push_back(i);
    // }
    // std::cout<<x.read_at(0)<<" "<<x.size()<<std::endl;
    // for(int i=0;i<x.size();i++){
    //     std::cout<<x.read_at(i)<<std::endl;
    // }
    //
    // std::cout<<x.pop_back()<<" "<<x.pop_back()<<std::endl;
    // x.push_back(100);
    // std::cout<<x.pop_back()<<std::endl;
    // int v = 1;
    // int* y = &v;
    // int z = *y;
    // std::cout<<y<<" "<<&z << std::endl;
}
