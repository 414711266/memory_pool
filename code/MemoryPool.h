#pragma once

#include<iostream>
#include <atomic>  // 为了将来引入原子操作，但暂时先用 FreeSlot*
#include <mutex>     // <<< 新增：为了 std::mutex 和 std::lock_guard
#include <thread>    // <<< 新增：为了 std::this_thread::get_id() (调试用)

// =====================================================================
// MemoryPool 类的定义 (与上一步基本一致)
// =====================================================================
namespace Karl_memoryPool {

#define MEMORY_POOL_NUM 64      // 定义了 HashBucket 将管理的 MemoryPool 实例的总数量。有64个不同大小的内存池。
#define SLOT_BASE_SIZE 8		// 定义了最小内存池的槽大小，也是其他内存池槽大小递增的基本单位。
#define MAX_SLOT_SIZE 512		// 定义了内存池能够处理的最大对象（槽）大小。如果请求的内存大小超过 MAX_SLOT_SIZE，HashBucket 将不再使用自定义内存池，而是回退到使用系统标准的 operator new 来分配内存。

struct Slot {
	std::atomic<Slot*> pNext;
};

class MemoryPool
{
public:
	// 构造时传入每个小块的固定大小 (slotSize) 和每个大块的大小 (blockSize)
	//MemoryPool() noexcept = default;
	MemoryPool(size_t slotSize = 0, size_t initialBlockSize = 4096);

	~MemoryPool();

	void* allocate(); // 不再传入size，因为slotSize是固定的
	void deallocate(void* ptr); // 不再传入size

	void init(size_t slotSize, size_t initialBlockSize = 4096); // 提供一个初始化方法
private:
	void allocateNewBlock(); // 申请新的大内存块

	// 辅助函数：计算指针p为了对齐到align字节需要填充多少字节
	static size_t padPointer(char* p, size_t align);

	// 原子操作的空闲链表
	// 将一个空闲槽（slot）原子地推入空闲链表头部。
	bool pushFreeList(Slot* slot);
	// 原子地弹出空闲链表头部节点。
	Slot* popFreeList();

	Slot* pHeadBlock_;     //指向第一个大内存块的指针
	Slot* curSlot_;        // 指向当前大内存块中下一个可分配的槽 (原 pCurrentSlot_)
	Slot* lastSlot_;       // 指向当前大内存块中最后一个可分配槽的结束边界 (原 pLastSlot_)

	// Slot* pFreeList_; // <<< 移除旧的非原子链表头
	//多线程可能同时修改 atomicFreeList_（如 pushFreeList 和 popFreeList）。
	std::atomic<Slot*> atomicFreeList_; // 新增：原子类型的空闲链表头

	size_t slotSize_;      // 每个分配单元（槽）的大小
	size_t blockSize_;     // 每个大内存块的大小

	std::mutex mutexForBlock_; //新增：用于保护大块分配和相关指针
};

// =====================================================================
// HashBucket 类的定义 (这是本步骤的新增内容)
// =====================================================================
class HashBucket {
public:
	// 初始化包含 MEMORY_POOL_NUM 个 MemoryPool 的静态数组
	static void initMemoryPool();

	// 根据请求的大小，分配内存
	static void* useMemory(size_t size);

	// 根据传入指针和原始大小，回收内存
	static void freeMemory(void* ptr, size_t size);

private:
	// 获取指定索引的 MemoryPool 实例的引用
	static MemoryPool& getMemoryPool(int index);
};

// =====================================================================
// newElement / deleteElement 模板函数定义 (这是本步骤的新增内容)
// =====================================================================
template<typename T, typename... Args>
T* newElement(Args&&... args) {
	T* p = nullptr;
	// 1. 使用 HashBucket 分配原始内存
	if ((p = reinterpret_cast<T*>(HashBucket::useMemory(sizeof(T)))) != nullptr) {
		// 2. 在分配的内存上使用 placement new 构造对象
		new(p) T(std::forward<Args>(args)...);
	}
	return p;
}

template<typename T>
void deleteElement(T* p) {
	if (p) {
		// 1. 显式调用对象的析构函数
		p->~T();
		// 2. 使用 HashBucket 回收内存
		HashBucket::freeMemory(reinterpret_cast<void*>(p), sizeof(T));
	}
}

} // namespace Karl_memoryPool