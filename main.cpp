#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <random>
#include <string>
#include <algorithm>
#include <map>


#include "descriptors.h"
#include "lf_vec.h"

enum class Op {
    Read,
    Write,
    Push,
    Pop 
};

std::mutex mtx;
std::vector<int> locked_vector(FIRST_BUCKET_SIZE);
std::vector<std::thread> threads;
std::vector<std::vector<Op>> sequences;

int PER_THREAD_OPERATIONS = 500000; // as stated in paper
int SEED = 42;
bool LF = false;
bool LEAK = false;

std::string op_to_string(Op op) {
    switch(op) {
        case Op::Read: return "read";
        case Op::Write: return "write";
        case Op::Push: return "push_back";
        case Op::Pop: return "pop_back";
        default: return "unknown";
    }
}

std::vector<Op> generate_operation_sequence(
    int total_ops,
    std::map<Op, int> percentage_map,
    unsigned seed
) {
    std::vector<Op> sequence;

    // Calculate exact counts
    int total = 0;
    for (const auto& [op, percent] : percentage_map) {
        int count = (total_ops * percent) / 100;
        sequence.insert(sequence.end(), count, op);
        total += count;
    }

    // Fill remainder due to rounding
    while (total < total_ops) {
        sequence.push_back(Op::Read); // Or pick any fallback op
        total++;
    }

    // Shuffle deterministically
    std::mt19937 rng(seed);
    std::shuffle(sequence.begin(), sequence.end(), rng);

    return sequence;
}

void lf_work(int thread_id,lockfree::Vector<int>& lf_vec){
    int v;
    std::vector<Op>& sequence = sequences[thread_id];

    lf_vec.set_id(thread_id);
    while(!sequence.empty()){
        Op curr_op = sequence.back();
        sequence.pop_back();

        switch(curr_op){
            case Op::Read:
                v = lf_vec.read_at(0);
            break;
            case Op::Write:
                lf_vec.write_at(0,thread_id);
            break;
            case Op::Pop:
                if(LEAK){
                    lf_vec.pop_back_LEAK();
                }else{
                    lf_vec.pop_back();
                }
            break;
            case Op::Push:
                if(LEAK){
                    lf_vec.push_back_LEAK(thread_id);
                }else{
                    lf_vec.push_back(thread_id);
                }
            break;
        }
    }
}

void mtx_work(int thread_id){
    int v;
    std::vector<Op>& sequence = sequences[thread_id];

    while(!sequence.empty()){
        Op curr_op = sequence.back();
        sequence.pop_back();

        mtx.lock();
        switch(curr_op){
            case Op::Read:
                v = locked_vector[0];
            break;
            case Op::Write:
                locked_vector[1] = 10;
            break;
            case Op::Pop:
                locked_vector.pop_back();
            break;
            case Op::Push:
                locked_vector.push_back(1);
            break;
        }
        mtx.unlock();
    }
}

int main(int argc, char* argv[]){
    int read_prob = 100;
    int write_prob = 0;
    int push_prob = 0;
    int pop_prob = 0;

    for(int i=1; i<argc; i++){
        std::string arg(argv[i]);
        if(arg =="-lf")
            LF = true;
        if(arg == "-l")
            LF = false;
        if(arg == "-leak")
            LEAK = true;
        if(arg == "-threads")
            MAX_THREADS = std::atoi(argv[i+1]);
        if(arg == "-pools")
            MAX_POOLS = std::atoi(argv[i+1]);
        if(arg == "-ops")
            PER_THREAD_OPERATIONS = std::atoi(argv[i+1]);
        if(arg == "-seed")
            SEED = std::atoi(argv[i+1]);  
        if(arg == "-push")
            push_prob = std::atoi(argv[i+1]);
        if(arg == "-pop")
            pop_prob = std::atoi(argv[i+1]);
        if(arg == "-write")
            write_prob = std::atoi(argv[i+1]);
        if(arg == "-read")
            read_prob = std::atoi(argv[i+1]);
    } 

    
    lockfree::Vector<int> lf_vec;
    printf("starting simulation\nThreads: %d\nLock Free: %d\nLeak: %d\nOperations: %d\nPools: %d\nSeed: %d\n\n",MAX_THREADS,LF,LEAK,PER_THREAD_OPERATIONS,MAX_POOLS,SEED);
    std::map<Op, int> percentages = {
        {Op::Read, read_prob},
        {Op::Write, write_prob},
        {Op::Push, push_prob},
        {Op::Pop, pop_prob}
    };
    assert(read_prob + write_prob + push_prob + pop_prob == 100);
    std::cout<<"Operation Probabilities\n";
    for(const auto& pair: percentages){
        std::cout<<op_to_string(pair.first)<<": "<<pair.second<<"%\n";
    }

    // generate sequences for each thread
    for(int i=0; i<MAX_THREADS;i++){
        sequences.push_back(generate_operation_sequence(PER_THREAD_OPERATIONS, percentages, SEED+i));
    }

    auto start = std::chrono::high_resolution_clock::now();
    for(int i=0;i<MAX_THREADS; i++){
        if(LF){
            threads.push_back(std::thread(lf_work,i,std::ref(lf_vec)));
        }else{
            threads.push_back(std::thread(mtx_work,i));
        }
    }

    for(int i=0;i<threads.size();i++){
        threads[i].join();
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end-start);
    std::cout<<"total time: "<<duration.count()<<"ms\n";
 
    return 0;
} 