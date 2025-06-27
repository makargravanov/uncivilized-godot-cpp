//
// Created by Alex on 27.06.2025.
//

#ifndef MISC_H
#define MISC_H
#include <thread>

inline std::size_t getThreadIdHash() {
    return std::hash<std::thread::id>{}(std::this_thread::get_id());
}

#endif //MISC_H
