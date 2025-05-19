#include <iostream>
#include "lf_vec.h"

int main(){
    std::cout << "hello world" << std::endl;
    lockfree::Vector<int> x;
    std::cout<<x.read_at(0)<<std::endl;
    // x.write_at(0,1);
    // x.complete_write(new lockfree::WriteDescriptor<int>(0, 1, 0));

    for(int i=10;i<20;i++){
        x.push_back(i);
    }
    std::cout<<x.read_at(0)<<" "<<x.size()<<std::endl;
    for(int i=0;i<x.size();i++){
        std::cout<<x.read_at(i)<<std::endl;
    }

    std::cout<<x.pop_back()<<" "<<x.pop_back()<<std::endl;
    int v = 1;
    int* y = &v;
    int z = *y;
    std::cout<<y<<" "<<&z << std::endl;
}
