//
// Created by Alex on 23.01.2026.
//

#ifndef GODOTPTR_H
#define GODOTPTR_H

#include <utility>

#include <godot_cpp/core/memory.hpp>

template<typename T>
class GodotPtr {
public:
    GodotPtr() : ptr(nullptr) {}

    explicit GodotPtr(T* p) : ptr(p) {}

    ~GodotPtr() {
        reset();
    }

    GodotPtr(const GodotPtr&) = delete;
    GodotPtr& operator=(const GodotPtr&) = delete;

    GodotPtr(GodotPtr&& other) noexcept : ptr(other.ptr) {
        other.ptr = nullptr;
    }

    GodotPtr& operator=(GodotPtr&& other) noexcept {
        if (this != &other) {
            reset();
            ptr = other.ptr;
            other.ptr = nullptr;
        }
        return *this;
    }

    T* get() const { return ptr; }
    T* operator->() const { return ptr; }
    T& operator*() const { return *ptr; }

    explicit operator bool() const { return ptr != nullptr; }

    T* release() {
        T* temp = ptr;
        ptr = nullptr;
        return temp;
    }

    void reset(T* p = nullptr) {
        if (ptr) {
            godot::memdelete(ptr);
        }
        ptr = p;
    }


private:
    T* ptr;
};




#endif //GODOTPTR_H
