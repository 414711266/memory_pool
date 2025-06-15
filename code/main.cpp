#include "MemoryPool.h"
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

// 引入命名空间
using namespace Karl_memoryPool;

// 定义几个不同大小的测试类
class P1 { int id_[1]; };                                 // sizeof: 4,  使用8字节池
class P2 { int id_[5]; };                                 // sizeof: 20, 使用24字节池
class P3 { int id_[10]; };                                // sizeof: 40, 使用40字节池
class P4 { int id_[20]; };                                // sizeof: 80, 使用80字节池
class P5 { int id_[128]; };                               // sizeof: 512,使用512字节池
class P6 { int id_[130]; };                               // sizeof: 520, >512, 使用 operator new

// 使用 chrono 来计时
void BenchmarkMemoryPool(size_t ntimes, size_t nworks, size_t rounds) {
    std::vector<std::thread> vthread(nworks);
    std::atomic<long long> total_duration(0);

    for (size_t k = 0; k < nworks; ++k) {
        vthread[k] = std::thread([&]() {
            long long local_duration = 0;
            for (size_t j = 0; j < rounds; ++j) {
                auto start = std::chrono::high_resolution_clock::now();
                for (size_t i = 0; i < ntimes; ++i) {
                    deleteElement(newElement<P1>());
                    deleteElement(newElement<P2>());
                    deleteElement(newElement<P3>());
                    deleteElement(newElement<P4>());
                    deleteElement(newElement<P5>());
                    deleteElement(newElement<P6>()); // 测试大于MAX_SLOT_SIZE的情况
                }
                auto end = std::chrono::high_resolution_clock::now();
                local_duration += std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            }
            total_duration += local_duration;
            });
    }
    for (auto& t : vthread) {
        t.join();
    }
    printf("%zu个线程并发执行%zu轮次，每轮次new/delete %zu次(6种对象)，内存池总计花费：%lld ms\n",
        nworks, rounds, ntimes, total_duration.load());
}

void BenchmarkNew(size_t ntimes, size_t nworks, size_t rounds) {
    std::vector<std::thread> vthread(nworks);
    std::atomic<long long> total_duration(0);

    for (size_t k = 0; k < nworks; ++k) {
        vthread[k] = std::thread([&]() {
            long long local_duration = 0;
            for (size_t j = 0; j < rounds; ++j) {
                auto start = std::chrono::high_resolution_clock::now();
                for (size_t i = 0; i < ntimes; ++i) {
                    delete new P1();
                    delete new P2();
                    delete new P3();
                    delete new P4();
                    delete new P5();
                    delete new P6();
                }
                auto end = std::chrono::high_resolution_clock::now();
                local_duration += std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            }
            total_duration += local_duration;
            });
    }
    for (auto& t : vthread) {
        t.join();
    }
    printf("%zu个线程并发执行%zu轮次，每轮次new/delete %zu次(6种对象)，系统new总计花费：%lld ms\n",
        nworks, rounds, ntimes, total_duration.load());
}


int main() {
    // ** 关键第一步：初始化内存池 **
    HashBucket::initMemoryPool();

    const size_t ntimes = 10000;
    const size_t nworks = 4;
    const size_t rounds = 10;

    std::cout << "\n开始测试内存池性能..." << std::endl;
    BenchmarkMemoryPool(ntimes, nworks, rounds);

    std::cout << "\n===========================================================================" << std::endl;

    std::cout << "\n开始测试系统 new/delete 性能..." << std::endl;
    BenchmarkNew(ntimes, nworks, rounds);

    std::cout << "\n测试结束。" << std::endl;

    return 0;
}