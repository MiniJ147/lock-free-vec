#include <iostream>
#include "lf_vec.h"

int main(){
    std::cout << "hello world" << std::endl;
    lockfree::Vector<int> x;
    std::cout<<x.read_at(0)<<std::endl;
    x.write_at(0,1);
    std::cout<<x.read_at(0)<<std::endl;
}
