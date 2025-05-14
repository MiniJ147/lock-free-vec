#ifndef LF_VEC_H
#define LF_VEC_H
#include <iostream>

namespace lf {
// contains the logic and functions necessary to complete vector operations
// what the user will use at an abstract level
template <typename T>
class Vector {
private:
    // pointer array to valid blocks of memory 
    T** data; 

    T* at(size_t idx);
public:
    Vector();

    // vector functions
    void resize(size_t size);
    size_t push_back(T);
    T pop_back();

    // random accesses
    void write_at(size_t idx, T val);
    T read_at(size_t idx);

    // other
    size_t size();
};

// WriteDescriptor enforces the semantics of pop_back and push_back operations
// allow for multiple memory locations to be modified in a "single" operation
template <typename T>
class WriteDescriptor{
public:
    T old_val;
    T new_val;
    T* loc;
    bool completed;
};

// Descriptor which holds an optional reference to a write descriptor
// our Descriptor object guarantes the semantics of pop_back and push_back operations
template <typename T>
class Descriptor{
public: 
    WriteDescriptor<T>* write;
    size_t size;
    size_t counter; // reference counter
};

};

#endif
