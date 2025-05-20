#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include "descriptors.h"
#include "lf_vec.h"

#define PER_THREAD_OPERATIONS 500000 // as stated in paper

enum OPS {
    READ,
    WRITE,
    PUSH,
    POP
};

std::atomic<bool> cancel_flag(false);

std::vector<int> x2(8); // match on first bucket
std::mutex mtx;

lockfree::Vector<int> x;
// void work(int id){
    // int lx = 0;
    // int pops = 0;
    // x.set_id(id);
    // while(!cancel_flag){ 
    //     x.push_back(id);
        // if(id & 1 && x.size() > 1){ 
        //     x.pop_back();
        //     pops++;
        // }
        // lx++;
        // std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    // }
    // std::this_thread::sleep_for(std::chrono::seconds(id));
    // std::cout<<"id: "<<id<<" push: "<<lx<<" pops: "<<pops<<std::endl;
    // std::this_thread::sleep_for(std::chrono::seconds(2));
// }
void work(int id, OPS op_code, bool leak){
    x.set_id(id);
    int ops = PER_THREAD_OPERATIONS;
    while(ops > 0){
        switch(op_code){
            case READ:
                x.read_at(0);
            break;
            case WRITE:
                x.write_at(1,1);
            break;
            case PUSH:
                if(leak)
                    x.push_back_LEAK(2);
                else
                    x.push_back(2);
            break;
            case POP:
                if(leak)
                    x.pop_back_LEAK();
                else
                    x.pop_back();
            break;
        }
        ops--;
    }

}


// void work2(){
//     while(!cancel_flag){
//         mtx.lock();
//         x2.push_back(1);
//         mtx.unlock();
//     }
// }
void work2(OPS op_code){
    int ops = PER_THREAD_OPERATIONS;
    int v;
    while(ops > 0){
        mtx.lock();
        switch(op_code){
            case READ:
                v = x2[0];
                break;
            case WRITE:
                x2[1] = 1;
                break;
            case PUSH:
                x2.push_back(2);
                break;
            case POP:
                x2.pop_back();
                break;
        }
        ops--;
        mtx.unlock();
    }

}

int main(){
    // std::cout << "Threads: "<< MAX_THREADS << std::endl;
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

    bool LF = true;
    bool LEAK = true;
    auto start = std::chrono::high_resolution_clock::now();
    for(int i=0; i<MAX_THREADS;i++){
        if(i < 3){ // 2 pushes
            if(LF)
                threads.push_back(std::thread(work,i,PUSH,LEAK));
            else
                threads.push_back(std::thread(work2,PUSH));
        }else if(i<8){ // 1 write
            if(LF)
                threads.push_back(std::thread(work,i,WRITE,LEAK));
            else
                threads.push_back(std::thread(work2,WRITE));

            // threads.push_back(std::thread(work,i,WRITE));
            // threads.push_back(std::thread(work2,WRITE));
        }else{ // 3 readers
            if(LF)
                threads.push_back(std::thread(work,i,READ,LEAK));
            else
                threads.push_back(std::thread(work2,READ));

            // threads.push_back(std::thread(work2,READ));
        } // one pop
            // threads.push_back(std::thread(work2,POP));
        // }
        // threads.push_back(std::thread(work2,PUSH));

    }

    // cancel_flag = true;
    // std::cout<<"starting cancel\n";
    for(int i=0; i<MAX_THREADS;i++){
        threads[i].join();
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end-start);
    std::cout<<"Threads: "<< MAX_THREADS << " Time: "<<duration.count()<<"ms"<<std::endl;

    // std::cout<<"sim done...\n";
    // std::cout<<"vec size: "<<x.size()<<std::endl;
    // std::cout<<"vec size: "<<x2.size()<<std::endl;

    // int check_ans[MAX_THREADS] = {0};
    // for(size_t i=0; i<x.size();i++){
    //     check_ans[x.read_at(i)]+=1;
    // }

    // for(int i=0;i<MAX_THREADS;i++){
    //     std::cout<<i<<": "<<check_ans[i]<<std::endl;
    // }
    // std::cout<<"\n";


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
