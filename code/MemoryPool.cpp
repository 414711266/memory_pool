#include "MemoryPool.h"
#include <cassert>
// #include <new>

namespace Karl_memoryPool {

// =====================================================================
// MemoryPool 类的实现 (与上一步完全一致)
// =====================================================================

MemoryPool::MemoryPool(size_t slotSize, size_t initialBlockSize)
	: pHeadBlock_(nullptr),
	curSlot_(nullptr),
	lastSlot_(nullptr),
	atomicFreeList_(nullptr), // 初始化原子变量
	slotSize_(0), // 将在 init 中设置
	blockSize_(0)  // 将在 init 中设置
	// this->mutexForBlock_() // 默认构造即可
{
	if (slotSize > 0) { // 如果构造时传入了 slotSize，则初始化
		this->init(slotSize, initialBlockSize);
	}
}

void MemoryPool::init(size_t slotSize, size_t initialBlockSize)
{
	assert(slotSize >= sizeof(Slot) && "slotSize must be at least sizeof(Slot)");
	// 如果使用 slotSize_ / sizeof(Slot) 这种算术，最好保证 slotSize_ 是 sizeof(Slot) 的整数倍
	assert(slotSize % sizeof(Slot) == 0 && "slotSize must be a multiple of sizeof(Slot)");
	assert(initialBlockSize > sizeof(Slot) && "initialBlockSize too small");

	this->slotSize_ = slotSize;
	this->blockSize_ = initialBlockSize;

	this->pHeadBlock_ = nullptr;
	this->curSlot_ = nullptr;
	this->lastSlot_ = nullptr;
	this->atomicFreeList_.store(nullptr, std::memory_order_relaxed); // 原子地设置为nullptr

	std::cout << "MemoryPool: 初始化完成。槽大小: " << this->slotSize_
		<< " 字节, 大块大小: " << this->blockSize_ << " 字节。" << std::endl;
}



MemoryPool::~MemoryPool() {
	Slot* currentBlock = this->pHeadBlock_;
	while (currentBlock != nullptr) {
		// 对于大块链表的 next，它不是并发访问的，析构时是单线程
		// 所以可以用 load(relaxed) 或者直接假设其 pNext 是非原子的 Slot*
		// 但我们 Slot 结构里的 pNext 已经是 atomic 了，所以用 load
		Slot* nextBlock = currentBlock->pNext.load(std::memory_order_relaxed);
		operator delete(reinterpret_cast<void*>(currentBlock));
		currentBlock = nextBlock;
	}
	std::cout << "MemoryPool: 所有内存块已释放。" << std::endl;
}

// 实现 padPointer 静态方法
size_t MemoryPool::padPointer(char* p, size_t align) {
	// 使用 uintptr_t 来确保整数类型足够大以存储指针的数值
	uintptr_t address = reinterpret_cast<uintptr_t>(p);
	return (align - (address % align)) % align;
}

// allocateNewBlock 方法本身不需要再加锁，因为它将被 allocate() 在持有锁的情况下调用
void MemoryPool::allocateNewBlock()
{
	char* newBlockStart = reinterpret_cast<char*>(operator new(this->blockSize_));
	if (newBlockStart == nullptr) {
		std::cerr << "MemoryPool: (线程 " << std::this_thread::get_id() 
			<< ") allocateNewBlock 失败，无法分配大块内存 (" 
			<< this->blockSize_ << " 字节)。" << std::endl;
		return;
	}
	// std::cout << "MemoryPool: (线程 " << std::this_thread::get_id() << ") 成功申请新大块内存，地址: " << static_cast<void*>(newBlockStart) << std::endl;

	// 2. 将新块头插到 pHeadBlock_ 链表中
	Slot* newBlockHeader = reinterpret_cast<Slot*>(newBlockStart);
	// newBlockHeader->pNext 的赋值是在当前线程栈上或堆上新分配的内存，没有并发问题
	// newBlockHeader->pNext = this->pHeadBlock_;
	// store 是原子操作，确保其他线程可见。
	newBlockHeader->pNext.store(this->pHeadBlock_, std::memory_order_relaxed); // 如果pNext是atomic
	this->pHeadBlock_ = newBlockHeader;


	// 计算实际用于槽分配的内存区域的起始点 (body)
	// 大块的头部 sizeof(Slot) 字节被用于链接 (newBlockHeader->pNext)
	char* body = newBlockStart + sizeof(Slot);
	size_t padding = MemoryPool::padPointer(body, this->slotSize_);

	this->curSlot_ = reinterpret_cast<Slot*>(body + padding);
	this->lastSlot_ = reinterpret_cast<Slot*>(newBlockStart + this->blockSize_ - this->slotSize_ + 1);

	// 你项目中 allocateNewBlock 里有 freeList_ = nullptr;
	// 我们现在有了原子的 atomicFreeList_。如果仍要清空：
	// this->atomicFreeList_.store(nullptr, std::memory_order_relaxed);
	// 但如前讨论，这可能导致问题。我们先不加，除非后续证明在你项目中这种行为的必要性。
	// 尤其是在CAS循环的popFreeList失败后才调用allocateNewBlock，此时freeList理论上应为空。
}

// 实现原子的 pushFreeList (参考你项目)
bool MemoryPool::pushFreeList(Slot* slot) {
	if (slot == nullptr) return false;
	while (true) {
		Slot* oldHead = this->atomicFreeList_.load(std::memory_order_relaxed); // 读取当前链表头
		slot->pNext.store(oldHead, std::memory_order_relaxed); // 新节点的next指向旧头
		// compare_exchange_weak  比较 atomicFreeList_ 的当前值是否等于 oldHead，若是则替换为 slot。
		// 若失败（其他线程修改了 atomicFreeList_），重新循环。
		if (this->atomicFreeList_.compare_exchange_weak(oldHead, slot,
			//std::memory_order_release：确保写操作对后续读可见。
			//std::memory_order_relaxed：无内存屏障，性能最优。
			std::memory_order_release,
			std::memory_order_relaxed)) { 
			return true; // 成功将新节点设为头
		}
		// CAS失败则 oldHead 已被更新为当前 atomicFreeList_ 的值，循环重试
	}
}

// 实现原子的 popFreeList (参考你项目)
Slot* MemoryPool::popFreeList() {
	while (true) {
		Slot* oldHead = this->atomicFreeList_.load(std::memory_order_acquire); // 读取当前头
		if (oldHead == nullptr) {
			return nullptr; // 链表为空
		}
		// 在访问 oldHead->pNext 之前，oldHead 必须是有效的。
		// 其他线程可能已经弹出了 oldHead 并将其返回给系统或重用，
		// 这是一个经典的ABA问题点，如果Slot被立即重用且地址相同。
		// 你项目中的Slot::next是atomic<Slot*>，读取它需要load
		Slot* newHead = oldHead->pNext.load(std::memory_order_relaxed);

		// 尝试用 newHead替换 oldHead 作为链表头
		if (this->atomicFreeList_.compare_exchange_weak(oldHead, newHead,
			std::memory_order_acquire, // 或者 acq_rel
			std::memory_order_relaxed)) {
			return oldHead; // 成功，返回弹出的节点
		}
		// CAS失败则 oldHead 已被更新，循环重试
	}
}

void* MemoryPool::allocate() 
{
	// 步骤 1: 尝试从原子的 freeList 分配
	Slot* slotFromFreeList = this->popFreeList();
	if (slotFromFreeList != nullptr) {
		// std::cout << "MemoryPool: (线程 " << std::this_thread::get_id() << ") 从 atomicFreeList 分配于 " << static_cast<void*>(slotFromFreeList) << std::endl;
		return static_cast<void*>(slotFromFreeList);
	}

	// 步骤 2: freeList 为空，则从大块内存分配 (加锁)
	{ // 创建作用域，用于 std::lock_guard
		std::lock_guard<std::mutex> lock(this->mutexForBlock_);
		// 在锁内可以再次尝试popFreeList，以减少持有锁时新块的分配。
		// 但这会稍微复杂化逻辑，你项目中没有这样做，我们保持一致。
		// Slot* slotAfterLock = this->popFreeList();
		// if (slotAfterLock) return static_cast<void*>(slotAfterLock);

		if (this->curSlot_ == nullptr || this->curSlot_ >= this->lastSlot_) {
			this->allocateNewBlock();
			// 检查 allocateNewBlock 是否成功 (比如 newBlockStart 分配失败导致 curSlot_ 未更新)
			if (this->curSlot_ == nullptr || this->curSlot_ >= this->lastSlot_) {
				std::cerr << "MemoryPool: (线程 " << std::this_thread::get_id() << ") 内存严重不足，allocateNewBlock后仍无法分配。" << std::endl;
				return nullptr; // 释放锁并返回
			}
		}

		Slot* allocatedSlot = this->curSlot_;
		this->curSlot_ = this->curSlot_ + (this->slotSize_ / sizeof(Slot));
		// std::cout << "MemoryPool: (线程 " << std::this_thread::get_id() << ") 从主块分配于 " << static_cast<void*>(allocatedSlot) << std::endl;
		return static_cast<void*>(allocatedSlot);
	} // lock_guard 在此析构，互斥锁被释放
}

// deallocate 方法在这一步仍然是线程不安全的
void MemoryPool::deallocate(void* ptr) {
	if (ptr == nullptr) {
		return;
	}

	Slot* slotToFree = static_cast<Slot*>(ptr);
	this->pushFreeList(slotToFree); // 使用原子的push操作
	// std::cout << "MemoryPool: (线程 " << std::this_thread::get_id() << ") 回收 " << static_cast<void*>(ptr) << " 到atomicFreeList" << std::endl;
}

// =====================================================================
// HashBucket 类的实现 (这是本步骤的新增内容)
// =====================================================================
MemoryPool& HashBucket::getMemoryPool(int index) {
	// 使用静态数组来存储所有的内存池实例,static 局部变量的生命周期会持续到整个程序结束。
	// 这种方式实现了单例模式，确保全局只有一组内存池
	// 当一个函数内部声明一个 static 变量时（无论是单个变量还是数组），这个变量的构造和初始化只会在程序第一次执行到该声明语句时发生一次。
	static MemoryPool memoryPools[MEMORY_POOL_NUM];
	assert(index >= 0 && index < MEMORY_POOL_NUM);
	return memoryPools[index];
}

void HashBucket::initMemoryPool() {
	for (int i = 0; i < MEMORY_POOL_NUM; ++i) {
		// 计算每个内存池应该管理的槽大小
		// 第0个池管理8字节，第1个池管理16字节，以此类推
		size_t slotSize = (i + 1) * SLOT_BASE_SIZE;
		getMemoryPool(i).init(slotSize);
	}
	std::cout << "HashBucket: 所有 " << MEMORY_POOL_NUM << " 个内存池已初始化。" << std::endl;
}

void* HashBucket::useMemory(size_t size) {
	if (size == 0) {
		return nullptr;
	}

	// 对于大于 MAX_SLOT_SIZE 的大内存，直接使用系统的 new
	if (size > MAX_SLOT_SIZE) {
		return operator new(size);
	}

	// 计算 size 应该放入哪个内存池
	// (size + 7) / 8 是一种向上取整的技巧，等价于 ceil(size / 8.0)
	// 例如 size=8, (8+7)/8 = 1. index=0.
	//      size=9, (9+7)/8 = 2. index=1.
	//      size=16, (16+7)/8 = 2. index=1.
	int index = static_cast<int>(((size + SLOT_BASE_SIZE - 1) / SLOT_BASE_SIZE) - 1);
	if (index < 0) { // 理论上 size > 0 不会发生
		index = 0;
	}
	return getMemoryPool(index).allocate();
}

void HashBucket::freeMemory(void* ptr, size_t size) {
	if (ptr == nullptr) {
		return;
	}

	// 对于大内存，直接使用系统的 delete
	if (size > MAX_SLOT_SIZE) {
		operator delete(ptr);
		return;
	}

	int index = static_cast<int>(((size + SLOT_BASE_SIZE - 1) / SLOT_BASE_SIZE) - 1);
	if (index < 0) {
		index = 0;
	}
	getMemoryPool(index).deallocate(ptr);
}

} // namespace Karl_memoryPool