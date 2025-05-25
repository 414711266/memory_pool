# memory_pool

## 什么是内存池？

内存池是一种预分配内存并进行重复利用的技术，通过减少频繁的动态内存分配与释放操作，从而提高程序运行效率。内存池通常预先分配一块大的内存区域，将其划分为多个小块，每次需要分配内存时直接从这块区域中分配，而不是调用系统的动态分配函数（如`new`或`malloc`）。简单来说就是**申请一块较大的内存块(**不够继续申请)，之后将这块内存的管理放在应用层执行，减少系统调用带来的开销。

### 为什么要做内存池

- 性能优化
  - **减少动态内存分配开销**：系统调用 malloc/new 和 free/delete 进行内存管理操作复杂，性能低。而内存池通过预分配内存，并以简单管理逻辑，能显著提升内存分配和释放效率。比如在一个频繁创建和销毁小对象的程序中，使用系统调用分配内存会因复杂操作导致效率低下，内存池则可避免此问题。
  - **避免内存碎片**：动态分配内存时，尤其是大量小对象频繁分配和释放，程序长时间运行会因申请内存块大小不定，产生大量内存碎片，降低程序和操作系统性能。内存池通过管理固定大小内存块，可有效防止碎片化。例如在图形渲染程序中，若频繁创建和销毁小的图形元素，使用内存池可避免内存碎片。
  - **降低系统调用频率**：像 malloc 这类系统级内存分配需进入内核态，频繁调用开销大。内存池减少系统调用频率，从而提高程序效率。例如在高并发的网络服务器中，若频繁使用系统调用分配内存，会因内核态切换开销大，而内存池可减少这种开销。
- **确定性（实时性）**：**稳定的分配时间**，使用内存池能让分配和释放操作耗时更可控、稳定，适用于对实时性要求严格的系统。如在自动驾驶汽车的实时控制系统中，需保证内存分配时间稳定，内存池就能满足这一需求。

### 内存池的应用场景

- 高频小对象分配
  - **游戏开发**：游戏中像粒子、子弹、NPC 等大量小对象频繁动态分配和释放，使用内存池可优化性能。例如在一款射击游戏中，大量子弹对象的创建和销毁，内存池可提升处理效率。
  - **网络编程**：网络编程里大量请求和响应对象（如消息报文）频繁创建和销毁，适合用内存池。如在一个高并发的 Web 服务器处理 HTTP 请求时，消息报文的处理可借助内存池。
  - **内存管理库**：一些容器或数据结构（如 std::vector 或 std::deque）内部可能用内存池优化分配性能。比如在使用 std::vector 存储大量小数据时，内存池可优化其内存分配。
- **实时系统**：在嵌入式设备或实时控制系统中，动态内存分配延迟可能影响实时性，内存池能提供确定性分配性能。例如在工业自动化控制的嵌入式系统中，内存池可确保实时响应。
- **高性能计算**：高性能计算程序中，频繁内存分配和释放影响性能，内存池可优化内存管理。比如在气象模拟这类高性能计算程序中，内存池可提升整体性能。
- **服务器开发**：数据库服务器、web 服务器等管理大量连接和请求，涉及大量内存分配，内存池能提升服务器性能。如大型数据库服务器处理大量客户端连接时，内存池可优化内存使用。

### 内存池在代码中的应用

- **替换动态开辟内存的系统调用**：对 new/malloc/delete/free 等系统调用进行替换，使用内存池机制来管理内存。例如在一个自定义的内存管理模块中，用内存池的函数替代系统的 new 和 delete 操作。
- **替换 STL 容器空间配置器**：对 STL 众多容器中的空间配置器 std::allocator 进行替换，使 STL 容器使用内存池的方式分配内存。比如让 std::vector 使用内存池分配内存，提高其性能。

### 内存池的缺点

- **初始内存占用**：内存池需预先分配较大内存区域，可能造成部分内存浪费。比如在一个小型程序中，预先分配了较大内存池，但实际使用量很少，就浪费了内存。

- **复杂性**：实现和调试内存池代码比直接使用 malloc/new 更复杂。例如内存池的内存分配算法、回收机制等实现起来较为复杂，调试时也更困难。

- **不适合大型对象**：对于大对象的分配，使用内存池可能不划算。比如在处理大型图像数据时，使用内存池可能不如直接使用系统内存分配高效。

## 实现步骤

从一个实际的需求场景出发，逐步构建和优化这个内存池项目。

### 背景与需求

假设你正在开发一个高性能的网络服务器。这个服务器需要处理大量的客户端连接，并且为每个连接或每个请求，都需要动态地创建和销毁一些小对象。比如：

- 为每个传入的数据包创建一个临时的`Packet`对象来存储信息。
- 为每个用户的短期会话创建一个`SessionInfo`对象。
- 在处理逻辑中，可能需要一些小块的缓冲内存。

这些对象的特点是：

1. **体积小**：可能只有几十或几百字节。
2. **生命周期短**：创建后很快就被销毁。
3. **申请释放频繁**：在高并发或高吞吐量的场景下，这种小对象的申请和释放会非常非常多。

**遇到的问题：** 如果直接使用C++的 `new` 和 `delete` (或者C语言的 `malloc` 和 `free`) 来管理这些小对象，可能会遇到性能瓶颈。因为：

- **系统调用开销**：`new/delete` 底层通常会涉及到操作系统内核的调用，这有固定的开销。
- **内存碎片**：频繁申请和释放不同大小的小块内存，容易导致内存碎片，使得后续难以找到合适大小的连续内存块。
- **查找开销**：通用的内存分配器为了管理整个堆内存，其内部查找可用内存块的算法可能对于小对象来说过于复杂和耗时。
- **多线程竞争**：在多线程环境下，全局的堆分配器通常需要加锁来保证线程安全，这会成为并发瓶颈。

**我们的初步目标：** 为了解决这个问题，我们希望设计一个**内存池 (Memory Pool)**。它的基本思想是：一次性向操作系统申请一大块内存，然后在这块大内存中手动管理小块内存的分配和回收。这样可以绕开大部分系统调用，减少碎片，并可能实现更高效的分配策略。

### 一、简单内存池，只分配，不回收的内存块

创建一个最基础的内存分配器，它能从预先分配的一大块内存中切出小块内存。为了简化，我们暂时不考虑内存回收和线程安全问题。

#### SimpleMemoryBlock.h

```c++
#pragma once

#include<iostream>

class SimpleMemoryBlock
{
public:
	SimpleMemoryBlock() noexcept = default;
	SimpleMemoryBlock(size_t blockSize = 4096);

	~SimpleMemoryBlock();

	void* allocate(size_t size);

private:
	char* pBlock;         // 指向内存块的指针
	size_t blockSize;   // 内存块的总大小
	char* pCurrent;   // 指向当前可分配位置的指针
};
```

#### SimpleMemoryBlock.cpp

```c++
#include "SimpleMemoryBlock.h"
// #include <new>

SimpleMemoryBlock::SimpleMemoryBlock(size_t blockSize)
	: blockSize(blockSize),
	pCurrent(nullptr)
{
	// 申请一大块内存
	pBlock = new(std::nothrow) char[blockSize];
	if (pBlock == nullptr) {
		// 实际项目中可能需要更健壮的错误处理，比如抛出异常
		std::cerr << "SimpleMemoryBlock: 初始分配 " << blockSize << " 字节内存失败!" << std::endl;
	}
	pCurrent = pBlock; // 初始时，可分配位置就是块的起始位置
	std::cout << "SimpleMemoryBlock: 成功初始化，分配了 " << blockSize << " 字节。" << std::endl;
}

SimpleMemoryBlock::~SimpleMemoryBlock()
{
	delete[] pBlock; // 释放整个内存块
	std::cout << "SimpleMemoryBlock: 内存块已释放。" << std::endl;
}

void* SimpleMemoryBlock::allocate(size_t size)
{
	if (pBlock == nullptr) { // 如果初始分配失败
		return nullptr;
	}

	// 暂时不做严格的边界检查，假设总能分配成功
	// 实际应该检查: pCurrent + size <= pBlock + blockSize
	if (pCurrent + size > pBlock + blockSize) {
		std::cerr << "SimpleMemoryBlock: 请求分配 " << size << " 字节失败，内存不足!" << std::endl;
		return nullptr; // 分配失败
	}

	void* allocated_memory = static_cast<void*>(pCurrent);
	pCurrent += size; // 移动指针，准备下一次分配
	// std::cout << "SimpleMemoryBlock: 分配了 " << size << " 字节。剩余: " << (pBlock + blockSize - pCurrent) << " 字节。" << std::endl;
	return allocated_memory;
}

```

#### main.cpp

```c++
// 预先向系统申请一大片内存，并交由应用层管理，在程序运行时，
// 内存的分配和回收都由应用层的内存池处理，从而减少系统调用。
// 2025年5月24日22:30:30

#include <iostream>
#include "SimpleMemoryBlock.h"

// sizeof为20
struct MyData {
    int id;
    char name[16];
};

int main()
{
    SimpleMemoryBlock block(1024); // 创建一个1KB的内存块
    // 第一次分配
    void* mem1 = block.allocate(sizeof(MyData));
    if (mem1) {
        MyData* data1 = static_cast<MyData*>(mem1);
        data1->id = 1;
        // strcpy(data1->name, "Object1"); // 注意strcpy的安全性
        snprintf(data1->name, sizeof(data1->name), "对象1"); // 使用 snprintf 更安全
        std::cout << "分配 MyData 于 " << data1 << ", ID: " << data1->id << ", 名称: " << data1->name << std::endl;
    }
    else {
        std::cout << "分配 mem1 失败" << std::endl;
    }

    // 第二次分配
    void* mem2 = block.allocate(100); // 分配100字节
    if (mem2) {
        std::cout << "分配 100 字节于 " << mem2 << std::endl;
    }
    else {
        std::cout << "分配 mem2 失败" << std::endl;
    }

    // 尝试分配一个会超出剩余空间的内存 (如果SimpleMemoryBlock够小，或者分配次数够多)
    void* mem3 = block.allocate(2000); // 尝试分配一个大于总块大小的内存 (或者大于剩余)
    if (mem3) {
        std::cout << "分配 2000 字节于 " << mem3 << std::endl;
    }
    else {
        std::cout << "分配 mem3 失败 (空间不足，符合预期)" << std::endl;
    }

    // 注意：这里分配的内存并没有单独释放，它们会在 block 对象析构时随着整个大块内存一起被释放。
    // 这就是这个最简单版本内存池的特点：“只分配，不回收单个对象”。

    return 0;
}
```

#### 局限性

- **无法回收单个对象**：我们只能分配，不能单独 `deallocate` 某个对象并重用那块内存。一旦分配出去，那块内存就一直被占用，直到整个 `SimpleMemoryBlock` 对象销毁。

- **内存浪费（对齐问题）**：我们现在是按需分配 `size` 大小的内存。如果CPU对某些数据类型的访问有对齐要求（比如 `int` 通常要求4字节对齐，`double` 要求8字节对齐），直接这样连续分配可能会导致未对齐的内存地址，从而引发性能下降甚至错误。我们分配的 `void*` 没有考虑这一点。

- **固定大小的内存块**：如果所有对象都分配完了，就不能再分配了，除非创建新的 `SimpleMemoryBlock`。

- **线程不安全**：如果多个线程同时调用 `block.allocate()`，`pCurrent += size;` 这行代码会有竞态条件，导致严重错误。

- **只能处理特定大小的对象吗？** 目前可以处理任意大小的请求（只要总容量够），但如果我们想针对特定大小的对象做优化（比如都分配8字节、16字节的块），现在的设计还不够。
