#include<proc/proc.h>
#include<proc/cpu.h>
#include<mm/vm.h>
#include<mm/malloc.h>
#include<sync/spinlock.h>
#include<lib/string.h>
#include<lib/stdio.h>
#include<debug/debug.h>
#include<lib/x64.h>
struct context{
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t rbx;
    uint64_t rbp;
    uint64_t rip;
    uint64_t padding;
};
struct proc procs[128];
struct thread threads[256];
struct spinlock ptablelock;
void switchstack(uint64_t* oldrsp, uint64_t* newrsp);
void kthread();
static void setidleprocess(){
    //把0号进程和0-(cpuno-1)号线程设置为空闲进程/线程
    procs[0].pgdir = kpgdir;
    procs[0].used = 1;
    for(uint64_t i = 0; i < cpuno; i++){
        threads[i].kstack = 0;
        threads[i].proc = &(procs[0]);
        threads[i].state = proc_runnable;
        cpus[i].thread = &(threads[i]);
    }
}
uint64_t firstthread(void* args){
    printf("This is first thread.\n");
    panic("");
    return 0;
}
struct proc* allocproc(){
    struct proc* ret = 0;
    uint64_t curpid = cpu->thread->proc->pid;
    acquire(&ptablelock);
    for(uint64_t i = curpid; i < 128; i++){
        if(!(procs[i].used)){
            procs[i].used = 1;
            ret = &(procs[i]);
            break;
        }
    }
    if(!ret){
        for(uint64_t i = 0; i < curpid; i++){
            if(!(procs[i].used)){
                procs[i].used = 1;
                ret = &(procs[i]);
                break;
            }
        }
    }
    release(&ptablelock);
    return ret;
}
void freeproc(struct proc* p){
    p->used = 0;
}
struct thread* allocthread(){
    struct thread* ret = 0;
    uint64_t curtid = cpu->thread->tid;
    acquire(&ptablelock);
    for(uint64_t i = curtid; i < 256; i++){
        if(!(threads[i].state)){
            threads[i].state = proc_sleeping;
            ret = &(threads[i]);
            break;
        }
    }
    if(!ret){
        for(uint64_t i = 0; i < curtid; i++){
            if(!(threads[i].state)){
                threads[i].state = proc_sleeping;
                ret = &(threads[i]);
                break;
            }
        }
    }
    release(&ptablelock);
    return ret;
}
void freethread(struct thread* t){
    t->state = proc_unused;
}
int64_t newthread(uint64_t newproc){
    uint64_t* kstack = (uint64_t*)alloc();
    if(kstack == 0){
        return -1;
    }
    uint64_t* pgdir = 0;
    if(newproc){
        pgdir = (uint64_t*)alloc();
        if(pgdir == 0){
            free((uint64_t)kstack);
            return -1;
        }
    }
    struct proc* p;
    if(newproc){
        p = allocproc();
        if(p == 0){
            free((uint64_t)kstack);
            free((uint64_t)pgdir);
            return -1;
        }
    }else{
        p = cpu->thread->proc;
    }
    struct thread* t = allocthread();
    if(t == 0){
        if(newproc){
            freeproc(p);
            free((uint64_t)pgdir);
        }
        free((uint64_t)kstack);
        return -1;
    }
    if(newproc){
        p->heaptop = 0;
        p->pgdir = pgdir;
        memcopy((char*)pgdir, (char*)kpgdir, 4096);
        p->stacktop = 0x0000800000000000;
    }else{
        t->proc = p;
    }
    t->kstack = kstack;
    memset((char*)kstack, 0, 4096);
    return t->tid;
}
void runthread(struct thread* t){
    if(t != cpu->thread){
        acquire(&ptablelock);
        struct thread* c = cpu->thread;
        cpu->thread = t;
        if(c->proc != t->proc){
            lcr3(k2p((uint64_t)(t->proc->pgdir)));
        }
        switchstack(&(c->rsp), &(t->rsp));
        release(&ptablelock);
    }
}
int64_t kernelthread(uint64_t (*fn)(void *), void *args, uint64_t newproc){
    int64_t newtid = newthread(newproc);
    if(newtid < 0){
        return newtid;
    }
    struct thread* t = &(threads[newtid]);
    struct context c;
    c.r15 = (uint64_t)fn;
    c.rbx = (uint64_t)args;
    c.rip = (uint64_t)kthread;
    t->rsp = (uint64_t)t->kstack + 4096 - sizeof(struct context);
    memcopy((char*)(t->rsp), (char*)(&c), sizeof(struct context));
    if(!newproc){
        t->proc = cpu->thread->proc;
    }else{
        t->proc->parent = cpu->thread->proc;
    }
    t->state = proc_runnable;
    runthread(t);
    return 0;
}
void procinit(){
    for(uint64_t i = 0; i < 256; i++){
        threads[i].tid = i;
        threads[i].kstack = 0;
        threads[i].proc = 0;
        threads[i].state = 0;
        threads[i].rsp = 0;
        threads[i].tf = 0;
    }
    for(uint64_t i = 0; i < 128; i++){
        procs[i].heaptop = 0;
        procs[i].parent = 0;
        procs[i].pgdir = 0;
        procs[i].pid = i;
        procs[i].used = 0;
        procs[i].stacktop = 0x0000800000000000;
    }
    setidleprocess();
    kernelthread(firstthread, 0, 0);
}


