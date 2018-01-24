#pragma once
#include<lib/util.h>
void mminit();
//内核虚拟地址->物理地址
static inline uint64_t k2p(uint64_t k){
    return k - 0xffff800000000000;
}
//物理地址->内核虚拟地址
static inline uint64_t p2k(uint64_t p){
    return p + 0xffff800000000000;
}
extern uint64_t* kpgdir;
