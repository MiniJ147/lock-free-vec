#ifndef DESCRIPTORS_H
#define DESCRIPTORS_H
#include <iostream>
#include <atomic>

template <typename T>
class WriteDescriptor{
public:
    T old_val;
    T new_val;
    int pos;
    bool completed;

    WriteDescriptor<T>(){};
    WriteDescriptor(T _old_val, T _new_val, int _pos)
    : old_val(_old_val), new_val(_new_val), pos(_pos), completed(false){}

    void replace(WriteDescriptor<T> _new){
        old_val = _new.old_val;
        new_val = _new.new_val;
        pos = _new.pos;
        completed = _new.completed;
    }
};

// Descriptor which holds an optional reference to a write descriptor
// our Descriptor object guarantes the semantics of pop_back and push_back operations
template <typename T>
class Descriptor{
public: 
    WriteDescriptor<T>* write = nullptr;
    size_t size;

    Descriptor() : write(nullptr), size(0){};
    Descriptor(WriteDescriptor<T>* _write, size_t _size): write(_write),size(_size){};

    bool write_op_pending(){
        return (this->write != nullptr && !this->write->completed);
    }

    void replace(Descriptor<T>& _new){
        write = _new.write;
        size = _new.size;
    }
};


#endif
