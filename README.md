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

### 二、简单的内存回收 - 侵入式链表管理空闲块

希望能够 `deallocate` 一个不再使用的内存块，并且让这块内存能够被后续的 `allocate` 请求再次使用。

#### 设计思路

**空闲链表 (Free List)**：当一块内存被释放时，不把它还给操作系统，而是把它链接到一个“空闲块列表”中。当需要分配内存时，我们首先检查这个空闲链表是否有合适的内存块，如果有，就直接取出来用。

**侵入式链表 (Intrusive Linked List)**：为了管理这些空闲块，我们需要在每个内存块的头部（或者说，把内存块本身“看作”一个链表节点）存储一个指向下一个空闲块的指针。这就是所谓的“侵入式”设计，因为链表的结构信息是直接存储在被管理的数据块内部的。

**`Slot` 结构**：我们定义一个简单的结构体，比如叫 `FreeSlot`，它至少包含一个指向下一个 `FreeSlot` 的指针。当我们分配一块内存时，其大小至少要能容纳一个 `FreeSlot` 结构。

```c++
struct FreeSlot {
	FreeSlot* pNext;
};
```

**分配逻辑修改 `allocate(size_t size)`**：

- 首先检查空闲链表。如果空闲链表不为空，并且链表头部的空闲块大小**恰好**等于（或者大于，但为了简单起见，我们先要求恰好等于）请求的 `size`，则从空闲链表中移除这个块并返回。
- 如果空闲链表为空或没有合适的块，再从 `pCurrent` 指向的大块内存中切分，同上一步。

**释放逻辑 `deallocate(void* ptr, size_t size)`**：

- 将 `ptr` 指向的内存块转换成 `FreeSlot*`。
- 将其插入到空闲链表的头部。

**简化**

- 假设我们管理的内存池是用来分配**固定大小**的对象的。这意味着 `allocate(size_t size)` 中的 `size` 总是同一个值，并且这个 `size` 必须大于等于 `sizeof(FreeSlot)`。

#### SimpleMemoryBlock.h

```c++
#pragma once

#include<iostream>

// 定义空闲节点的结构 (对应你项目中的 Slot)
struct FreeSlot {
	FreeSlot* pNext;
};

class SimpleMemoryBlock
{
public:
	SimpleMemoryBlock() noexcept = default;
	SimpleMemoryBlock(size_t slotSize, size_t initialBlockSize = 4096);

	~SimpleMemoryBlock();

	void* allocate(); // 不再传入size，因为slotSize是固定的
	void deallocate(void* ptr); // 不再传入size

private:
	char* pBlock;         // 指向内存块的指针
	size_t blockSize;   // 内存块的总大小
	char* pCurrent;   // 指向当前可分配位置的指针
	char* pEnd;           // 指向大内存块的末尾，用于边界检查

	FreeSlot* pFreeList;  // 指向空闲内存块链表的头部

	//内存池只分配固定大小的内存块 (slotSize_)。这个大小在内存池初始化时指定。
	size_t slotSize_;     // 每个分配单元（槽）的大小
};
```

#### SimpleMemoryBlock.cpp

```c++
#include "SimpleMemoryBlock.h"
#include <cassert>
// #include <new>

SimpleMemoryBlock::SimpleMemoryBlock(size_t slotSize, size_t initialBlockSize)
	: pBlock(nullptr),
	blockSize(initialBlockSize),
	pCurrent(nullptr),
	pEnd(nullptr),
	pFreeList(nullptr),
	slotSize_(slotSize)
{
	// 确保每个槽的大小至少能容纳一个 FreeSlot 指针
	assert(slotSize_ >= sizeof(FreeSlot) && "slotSize must be at least sizeof(FreeSlot)");

	pBlock = new (std::nothrow) char[blockSize];
	if (pBlock == nullptr) {
		std::cerr << "SimpleMemoryBlock: 初始分配 " << blockSize << " 字节内存失败!" << std::endl;
		// 在实际项目中，这里可能需要抛出异常或有更复杂的错误处理
		return;
	}

	pCurrent = pBlock;
	pEnd = pBlock + blockSize; // 计算大块内存的末尾
	std::cout << "SimpleMemoryBlock: 成功初始化，总容量 " << blockSize << " 字节, 单个槽大小 " << slotSize_ << " 字节。" << std::endl;

}



SimpleMemoryBlock::~SimpleMemoryBlock()
{
	delete[] pBlock; // 释放整个内存块
	std::cout << "SimpleMemoryBlock: 内存块已释放。" << std::endl;
}

void* SimpleMemoryBlock::allocate()
{
	// 1. 优先从空闲链表分配
	if (pFreeList != nullptr) {
		void* memory = static_cast<void*>(pFreeList);
		pFreeList = pFreeList->pNext; // 移动空闲链表头指针
		// std::cout << "SimpleMemoryBlock: 从空闲链表分配了 " << slotSize_ << " 字节。" << std::endl;
		return memory;
	}

	// 2. 空闲链表为空，则从大块内存中分配
	// 检查大块内存是否还有足够空间 (pCurrent 加上 slotSize_ 是否会超过 pEnd)
	if (pCurrent + slotSize_ <= pEnd) {
		void* memory = static_cast<void*>(pCurrent);
		pCurrent += slotSize_; // 移动当前大块内存的分配指针
		// std::cout << "SimpleMemoryBlock: 从主内存块分配了 " << slotSize_ << " 字节。" << std::endl;
		return memory;
	}

	// std::cout << "SimpleMemoryBlock: 内存不足，无法分配 " << slotSize_ << " 字节。" << std::endl;
	return nullptr; // 所有内存都用完了
}

void SimpleMemoryBlock::deallocate(void* ptr) {
	if (ptr == nullptr) {
		return;
	}

	// 将释放的内存块转换为 FreeSlot*，并将其加入空闲链表的头部
	FreeSlot* releasedSlot = static_cast<FreeSlot*>(ptr);
	releasedSlot->pNext = pFreeList;
	pFreeList = releasedSlot;
	// std::cout << "SimpleMemoryBlock: 回收了 " << slotSize_ << " 字节到空闲链表。" << std::endl;
}
```

#### main.cpp

```c++
// 预先向系统申请一大片内存，并交由应用层管理，在程序运行时，
// 内存的分配和回收都由应用层的内存池处理，从而减少系统调用。
// 2025年5月24日22:30:30

#include <iostream>
#include <vector>
#include "SimpleMemoryBlock.h"

struct MyFixedSizeData {
    int id;
    char data[12]; // 假设 MyFixedSizeData 大小为 16 字节 (int:4 + char[12]:12)
                   // 或者为了确保至少能放下 FreeSlot*，我们可以调整
                   // 如果 FreeSlot* 是8字节，那么char data[8] 即可使总大小为16
};
const size_t FIXED_DATA_SIZE = sizeof(MyFixedSizeData);


int main()
{
    // 假设 FreeSlot* 是 8 字节，MyFixedSizeData 是 16 字节
    // 我们创建一个内存池，每个槽16字节，总共能放比如 5 个这样的对象 (16 * 5 = 80 字节)
    // 为了测试方便，我们让 slotSize 直接等于 FIXED_DATA_SIZE
    // 但要确保 FIXED_DATA_SIZE >= sizeof(FreeSlot)
    if (FIXED_DATA_SIZE < sizeof(FreeSlot)) {
        std::cerr << "错误: MyFixedSizeData (" << FIXED_DATA_SIZE
            << " bytes) 太小，无法容纳 FreeSlot* (" << sizeof(FreeSlot)
            << " bytes)." << std::endl;
        return 1;
    }

    SimpleMemoryBlock pool(FIXED_DATA_SIZE, FIXED_DATA_SIZE * 5); // 池子能放5个对象

    std::vector<MyFixedSizeData*> allocatedObjects;

    // 1. 分配直到池满
    std::cout << "\n--- 阶段1: 分配直到池满 ---" << std::endl;
    for (int i = 0; i < 7; ++i) { // 尝试分配7个，但池子只能放5个
        MyFixedSizeData* obj = static_cast<MyFixedSizeData*>(pool.allocate());
        if (obj) {
            obj->id = i + 1;
            snprintf(obj->data, sizeof(obj->data), "Obj%d", obj->id);
            allocatedObjects.push_back(obj);
            std::cout << "分配: ID " << obj->id << " (" << obj->data << ") at " << obj << std::endl;
        }
        else {
            std::cout << "分配失败 (预期池满)。尝试分配第 " << (i + 1) << " 个对象。" << std::endl;
            break; // 池满了就停止
        }
    }

    std::cout << "\n当前已分配对象数量: " << allocatedObjects.size() << std::endl;

    // 2. 释放一些对象
    std::cout << "\n--- 阶段2: 释放部分对象 ---" << std::endl;
    if (allocatedObjects.size() >= 2) {
        MyFixedSizeData* objToFree1 = allocatedObjects[1]; // 释放第2个 (index 1)
        MyFixedSizeData* objToFree2 = allocatedObjects[3]; // 释放第4个 (index 3)
        std::cout << "准备释放: ID " << objToFree1->id << " at " << objToFree1 << std::endl;
        pool.deallocate(objToFree1);
        std::cout << "准备释放: ID " << objToFree2->id << " at " << objToFree2 << std::endl;
        pool.deallocate(objToFree2);
        // 从 vector 中移除，避免悬空指针 (实际应用中需要更小心的管理)
        // 这里简单处理，不从 vector 中移除，只是为了演示地址复用
    }

    // 3. 尝试重新分配，看是否复用了已释放的内存
    std::cout << "\n--- 阶段3: 尝试重新分配 ---" << std::endl;
    for (int i = 0; i < 3; ++i) { // 尝试再分配3个
        MyFixedSizeData* obj = static_cast<MyFixedSizeData*>(pool.allocate());
        if (obj) {
            obj->id = 100 + i; // 用新的 id 标记
            snprintf(obj->data, sizeof(obj->data), "ReObj%d", obj->id);
            // allocatedObjects.push_back(obj); // 如果上面没移除，这里要注意
            std::cout << "重新分配: ID " << obj->id << " (" << obj->data << ") at " << obj << std::endl;
            // 观察这里的 obj 地址是否和之前释放的地址相同
        }
        else {
            std::cout << "重新分配失败。尝试第 " << (i + 1) << " 次重新分配。" << std::endl;
            break;
        }
    }

    std::cout << "\n--- 测试结束 ---" << std::endl;
    // allocatedObjects 中的内存在 pool 析构时会被间接处理（因为整个pBlock被delete）
    // 但理想情况下，所有分配的对象都应该在使用完毕后 deallocate
    return 0;
}
```

#### 局限性

**单一内存块，无法扩容**：

- `SimpleMemoryBlock` 初始化时分配一块固定大小的原始内存 (`pBlock`)。一旦这块内存（包括从空闲链表回收的）都用完了，就无法再分配新的内存了，除非销毁重建整个 `SimpleMemoryBlock`。

**线程不安全**：

- `allocate()` 和 `deallocate()` 方法中对 `pFreeList`、`pCurrent` 的读写操作没有加锁。如果在多线程环境下，多个线程同时调用这些方法，会导致数据竞争，空闲链表和 `pCurrent` 的状态会错乱，可能导致同一块内存被分配给多个线程，或者导致程序崩溃。

#### 侵入式链表回收机制

单独定义一个`FreeSlot`结构体，用于实现侵入式链表。其中只包含了一个指针，为什么不直接用指针来替代呢？

**侵入式链表**是一种特殊的链表实现方式，其关键特点是：**链表节点的指针直接存储在被管理对象的内存空间内**，而不是为链表节点单独分配内存。

![image](img/内存池中的侵入式链表工作原理.png)

##### 特点优势

- **自文档化代码**：明确表明这块内存的用途是作为空闲链表节点
- 代码意图更明确，表示"这是一个空闲内存块链表节点"
- 使用结构体：编译器可以检查类型错误
- **扩展性**：可以轻松添加新字段（如块大小、调试信息等）

##### 工作流程

1. **分配**：优先从`pFreeList`指向的链表获取内存
   - 获取到的内存块前几个字节曾经存储`FreeSlot::pNext`
   - 这些字节会被用户数据覆盖（存储用户数据）
   - `pFreeList`指向next（向后移动，重新指向已释放的空块）
2. **释放**：将内存块转为`FreeSlot`并插入到链表头
   - 内存块的前几个字节被重新解释为`FreeSlot::pNext`
   - 用户数据仍然存在，但被"遗忘"（不再访问）
   - 该内存插入到链表头部

这种方法的巧妙之处在于：**当内存块不被用于存储用户数据时，同一块内存被重新用于存储链表信息**，实现了零额外内存开销的空闲块管理。

侵入式链表是高性能内存管理的关键技术，它被广泛应用于操作系统内核、内存分配器和高性能服务器等场景。

### 三、支持动态扩容 - 管理多个内存块

当前 `SimpleMemoryBlock`最大的问题之一是，它只有一个固定大小的初始内存块 (`pBlock`)。一旦这个块的内存（包括从空闲链表回收的）被完全分配出去，就无法再提供新的内存了，除非销毁并重建整个对象。当内存池中的当前内存块不足以满足分配请求时，能够自动向系统申请一块新的大内存块，并将这些内存块链接起来进行管理。这样，内存池的总容量就可以动态增长了。
