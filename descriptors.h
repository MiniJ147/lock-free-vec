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

    WriteDescriptor(T old_val, T new_val, int pos){
        this->old_val = old_val;
        this->new_val = new_val;
        this->pos = pos;
        this->completed = false;
    }
};

// Descriptor which holds an optional reference to a write descriptor
// our Descriptor object guarantes the semantics of pop_back and push_back operations
template <typename T>
class Descriptor{
public: 
    WriteDescriptor<T>* write = nullptr;
    size_t size;

    // reference counter logic
    std::atomic<size_t> counter; 
    std::atomic<bool> can_reference = true;

    Descriptor() : write(nullptr), size(0), counter(0){};
    Descriptor(WriteDescriptor<T>* _write, size_t _size, size_t _counter): write(_write),size(_size),counter(_counter){};

    void reference(){
        counter.fetch_add(1);
    }

    void reference_disconnect(){
        can_reference.store(false);
    }

    bool dereference(){
        size_t res = counter.fetch_add(-1);
        return res <= 1 && !can_reference; // last thread to hold a reference
    }

    bool write_op_pending(){
        return (this->write != nullptr && !this->write->completed);
    }
};


#endif
