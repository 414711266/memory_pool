#include "MemoryPool.h"
#include <cassert>
// #include <new>

namespace Karl_memoryPool {

// =====================================================================
// MemoryPool ���ʵ�� (����һ����ȫһ��)
// =====================================================================

MemoryPool::MemoryPool(size_t slotSize, size_t initialBlockSize)
	: pHeadBlock_(nullptr),
	curSlot_(nullptr),
	lastSlot_(nullptr),
	atomicFreeList_(nullptr), // ��ʼ��ԭ�ӱ���
	slotSize_(0), // ���� init ������
	blockSize_(0)  // ���� init ������
	// this->mutexForBlock_() // Ĭ�Ϲ��켴��
{
	if (slotSize > 0) { // �������ʱ������ slotSize�����ʼ��
		this->init(slotSize, initialBlockSize);
	}
}

void MemoryPool::init(size_t slotSize, size_t initialBlockSize)
{
	assert(slotSize >= sizeof(Slot) && "slotSize must be at least sizeof(Slot)");
	// ���ʹ�� slotSize_ / sizeof(Slot) ������������ñ�֤ slotSize_ �� sizeof(Slot) ��������
	assert(slotSize % sizeof(Slot) == 0 && "slotSize must be a multiple of sizeof(Slot)");
	assert(initialBlockSize > sizeof(Slot) && "initialBlockSize too small");

	this->slotSize_ = slotSize;
	this->blockSize_ = initialBlockSize;

	this->pHeadBlock_ = nullptr;
	this->curSlot_ = nullptr;
	this->lastSlot_ = nullptr;
	this->atomicFreeList_.store(nullptr, std::memory_order_relaxed); // ԭ�ӵ�����Ϊnullptr

	std::cout << "MemoryPool: ��ʼ����ɡ��۴�С: " << this->slotSize_
		<< " �ֽ�, ����С: " << this->blockSize_ << " �ֽڡ�" << std::endl;
}



MemoryPool::~MemoryPool() {
	Slot* currentBlock = this->pHeadBlock_;
	while (currentBlock != nullptr) {
		// ���ڴ������� next�������ǲ������ʵģ�����ʱ�ǵ��߳�
		// ���Կ����� load(relaxed) ����ֱ�Ӽ����� pNext �Ƿ�ԭ�ӵ� Slot*
		// ������ Slot �ṹ��� pNext �Ѿ��� atomic �ˣ������� load
		Slot* nextBlock = currentBlock->pNext.load(std::memory_order_relaxed);
		operator delete(reinterpret_cast<void*>(currentBlock));
		currentBlock = nextBlock;
	}
	std::cout << "MemoryPool: �����ڴ�����ͷš�" << std::endl;
}

// ʵ�� padPointer ��̬����
size_t MemoryPool::padPointer(char* p, size_t align) {
	// ʹ�� uintptr_t ��ȷ�����������㹻���Դ洢ָ�����ֵ
	uintptr_t address = reinterpret_cast<uintptr_t>(p);
	return (align - (address % align)) % align;
}

// allocateNewBlock ����������Ҫ�ټ�������Ϊ������ allocate() �ڳ�����������µ���
void MemoryPool::allocateNewBlock()
{
	char* newBlockStart = reinterpret_cast<char*>(operator new(this->blockSize_));
	if (newBlockStart == nullptr) {
		std::cerr << "MemoryPool: (�߳� " << std::this_thread::get_id() 
			<< ") allocateNewBlock ʧ�ܣ��޷��������ڴ� (" 
			<< this->blockSize_ << " �ֽ�)��" << std::endl;
		return;
	}
	// std::cout << "MemoryPool: (�߳� " << std::this_thread::get_id() << ") �ɹ������´���ڴ棬��ַ: " << static_cast<void*>(newBlockStart) << std::endl;

	// 2. ���¿�ͷ�嵽 pHeadBlock_ ������
	Slot* newBlockHeader = reinterpret_cast<Slot*>(newBlockStart);
	// newBlockHeader->pNext �ĸ�ֵ���ڵ�ǰ�߳�ջ�ϻ�����·�����ڴ棬û�в�������
	// newBlockHeader->pNext = this->pHeadBlock_;
	// store ��ԭ�Ӳ�����ȷ�������߳̿ɼ���
	newBlockHeader->pNext.store(this->pHeadBlock_, std::memory_order_relaxed); // ���pNext��atomic
	this->pHeadBlock_ = newBlockHeader;


	// ����ʵ�����ڲ۷�����ڴ��������ʼ�� (body)
	// ����ͷ�� sizeof(Slot) �ֽڱ��������� (newBlockHeader->pNext)
	char* body = newBlockStart + sizeof(Slot);
	size_t padding = MemoryPool::padPointer(body, this->slotSize_);

	this->curSlot_ = reinterpret_cast<Slot*>(body + padding);
	this->lastSlot_ = reinterpret_cast<Slot*>(newBlockStart + this->blockSize_ - this->slotSize_ + 1);

	// ����Ŀ�� allocateNewBlock ���� freeList_ = nullptr;
	// ������������ԭ�ӵ� atomicFreeList_�������Ҫ��գ�
	// this->atomicFreeList_.store(nullptr, std::memory_order_relaxed);
	// ����ǰ���ۣ�����ܵ������⡣�����Ȳ��ӣ����Ǻ���֤��������Ŀ��������Ϊ�ı�Ҫ�ԡ�
	// ��������CASѭ����popFreeListʧ�ܺ�ŵ���allocateNewBlock����ʱfreeList������ӦΪ�ա�
}

// ʵ��ԭ�ӵ� pushFreeList (�ο�����Ŀ)
bool MemoryPool::pushFreeList(Slot* slot) {
	if (slot == nullptr) return false;
	while (true) {
		Slot* oldHead = this->atomicFreeList_.load(std::memory_order_relaxed); // ��ȡ��ǰ����ͷ
		slot->pNext.store(oldHead, std::memory_order_relaxed); // �½ڵ��nextָ���ͷ
		// compare_exchange_weak  �Ƚ� atomicFreeList_ �ĵ�ǰֵ�Ƿ���� oldHead���������滻Ϊ slot��
		// ��ʧ�ܣ������߳��޸��� atomicFreeList_��������ѭ����
		if (this->atomicFreeList_.compare_exchange_weak(oldHead, slot,
			//std::memory_order_release��ȷ��д�����Ժ������ɼ���
			//std::memory_order_relaxed�����ڴ����ϣ��������š�
			std::memory_order_release,
			std::memory_order_relaxed)) { 
			return true; // �ɹ����½ڵ���Ϊͷ
		}
		// CASʧ���� oldHead �ѱ�����Ϊ��ǰ atomicFreeList_ ��ֵ��ѭ������
	}
}

// ʵ��ԭ�ӵ� popFreeList (�ο�����Ŀ)
Slot* MemoryPool::popFreeList() {
	while (true) {
		Slot* oldHead = this->atomicFreeList_.load(std::memory_order_acquire); // ��ȡ��ǰͷ
		if (oldHead == nullptr) {
			return nullptr; // ����Ϊ��
		}
		// �ڷ��� oldHead->pNext ֮ǰ��oldHead ��������Ч�ġ�
		// �����߳̿����Ѿ������� oldHead �����䷵�ظ�ϵͳ�����ã�
		// ����һ�������ABA����㣬���Slot�����������ҵ�ַ��ͬ��
		// ����Ŀ�е�Slot::next��atomic<Slot*>����ȡ����Ҫload
		Slot* newHead = oldHead->pNext.load(std::memory_order_relaxed);

		// ������ newHead�滻 oldHead ��Ϊ����ͷ
		if (this->atomicFreeList_.compare_exchange_weak(oldHead, newHead,
			std::memory_order_acquire, // ���� acq_rel
			std::memory_order_relaxed)) {
			return oldHead; // �ɹ������ص����Ľڵ�
		}
		// CASʧ���� oldHead �ѱ����£�ѭ������
	}
}

void* MemoryPool::allocate() 
{
	// ���� 1: ���Դ�ԭ�ӵ� freeList ����
	Slot* slotFromFreeList = this->popFreeList();
	if (slotFromFreeList != nullptr) {
		// std::cout << "MemoryPool: (�߳� " << std::this_thread::get_id() << ") �� atomicFreeList ������ " << static_cast<void*>(slotFromFreeList) << std::endl;
		return static_cast<void*>(slotFromFreeList);
	}

	// ���� 2: freeList Ϊ�գ���Ӵ���ڴ���� (����)
	{ // �������������� std::lock_guard
		std::lock_guard<std::mutex> lock(this->mutexForBlock_);
		// �����ڿ����ٴγ���popFreeList���Լ��ٳ�����ʱ�¿�ķ��䡣
		// �������΢���ӻ��߼�������Ŀ��û�������������Ǳ���һ�¡�
		// Slot* slotAfterLock = this->popFreeList();
		// if (slotAfterLock) return static_cast<void*>(slotAfterLock);

		if (this->curSlot_ == nullptr || this->curSlot_ >= this->lastSlot_) {
			this->allocateNewBlock();
			// ��� allocateNewBlock �Ƿ�ɹ� (���� newBlockStart ����ʧ�ܵ��� curSlot_ δ����)
			if (this->curSlot_ == nullptr || this->curSlot_ >= this->lastSlot_) {
				std::cerr << "MemoryPool: (�߳� " << std::this_thread::get_id() << ") �ڴ����ز��㣬allocateNewBlock�����޷����䡣" << std::endl;
				return nullptr; // �ͷ���������
			}
		}

		Slot* allocatedSlot = this->curSlot_;
		this->curSlot_ = this->curSlot_ + (this->slotSize_ / sizeof(Slot));
		// std::cout << "MemoryPool: (�߳� " << std::this_thread::get_id() << ") ����������� " << static_cast<void*>(allocatedSlot) << std::endl;
		return static_cast<void*>(allocatedSlot);
	} // lock_guard �ڴ����������������ͷ�
}

// deallocate ��������һ����Ȼ���̲߳���ȫ��
void MemoryPool::deallocate(void* ptr) {
	if (ptr == nullptr) {
		return;
	}

	Slot* slotToFree = static_cast<Slot*>(ptr);
	this->pushFreeList(slotToFree); // ʹ��ԭ�ӵ�push����
	// std::cout << "MemoryPool: (�߳� " << std::this_thread::get_id() << ") ���� " << static_cast<void*>(ptr) << " ��atomicFreeList" << std::endl;
}

// =====================================================================
// HashBucket ���ʵ�� (���Ǳ��������������)
// =====================================================================
MemoryPool& HashBucket::getMemoryPool(int index) {
	// ʹ�þ�̬�������洢���е��ڴ��ʵ��,static �ֲ��������������ڻ�������������������
	// ���ַ�ʽʵ���˵���ģʽ��ȷ��ȫ��ֻ��һ���ڴ��
	// ��һ�������ڲ�����һ�� static ����ʱ�������ǵ��������������飩����������Ĺ���ͳ�ʼ��ֻ���ڳ����һ��ִ�е����������ʱ����һ�Ρ�
	static MemoryPool memoryPools[MEMORY_POOL_NUM];
	assert(index >= 0 && index < MEMORY_POOL_NUM);
	return memoryPools[index];
}

void HashBucket::initMemoryPool() {
	for (int i = 0; i < MEMORY_POOL_NUM; ++i) {
		// ����ÿ���ڴ��Ӧ�ù���Ĳ۴�С
		// ��0���ع���8�ֽڣ���1���ع���16�ֽڣ��Դ�����
		size_t slotSize = (i + 1) * SLOT_BASE_SIZE;
		getMemoryPool(i).init(slotSize);
	}
	std::cout << "HashBucket: ���� " << MEMORY_POOL_NUM << " ���ڴ���ѳ�ʼ����" << std::endl;
}

void* HashBucket::useMemory(size_t size) {
	if (size == 0) {
		return nullptr;
	}

	// ���ڴ��� MAX_SLOT_SIZE �Ĵ��ڴ棬ֱ��ʹ��ϵͳ�� new
	if (size > MAX_SLOT_SIZE) {
		return operator new(size);
	}

	// ���� size Ӧ�÷����ĸ��ڴ��
	// (size + 7) / 8 ��һ������ȡ���ļ��ɣ��ȼ��� ceil(size / 8.0)
	// ���� size=8, (8+7)/8 = 1. index=0.
	//      size=9, (9+7)/8 = 2. index=1.
	//      size=16, (16+7)/8 = 2. index=1.
	int index = static_cast<int>(((size + SLOT_BASE_SIZE - 1) / SLOT_BASE_SIZE) - 1);
	if (index < 0) { // ������ size > 0 ���ᷢ��
		index = 0;
	}
	return getMemoryPool(index).allocate();
}

void HashBucket::freeMemory(void* ptr, size_t size) {
	if (ptr == nullptr) {
		return;
	}

	// ���ڴ��ڴ棬ֱ��ʹ��ϵͳ�� delete
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