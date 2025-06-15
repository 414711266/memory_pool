#pragma once

#include<iostream>
#include <atomic>  // Ϊ�˽�������ԭ�Ӳ���������ʱ���� FreeSlot*
#include <mutex>     // <<< ������Ϊ�� std::mutex �� std::lock_guard
#include <thread>    // <<< ������Ϊ�� std::this_thread::get_id() (������)

// =====================================================================
// MemoryPool ��Ķ��� (����һ������һ��)
// =====================================================================
namespace Karl_memoryPool {

#define MEMORY_POOL_NUM 64      // ������ HashBucket ������� MemoryPool ʵ��������������64����ͬ��С���ڴ�ء�
#define SLOT_BASE_SIZE 8		// ��������С�ڴ�صĲ۴�С��Ҳ�������ڴ�ز۴�С�����Ļ�����λ��
#define MAX_SLOT_SIZE 512		// �������ڴ���ܹ�����������󣨲ۣ���С�����������ڴ��С���� MAX_SLOT_SIZE��HashBucket ������ʹ���Զ����ڴ�أ����ǻ��˵�ʹ��ϵͳ��׼�� operator new �������ڴ档

struct Slot {
	std::atomic<Slot*> pNext;
};

class MemoryPool
{
public:
	// ����ʱ����ÿ��С��Ĺ̶���С (slotSize) ��ÿ�����Ĵ�С (blockSize)
	//MemoryPool() noexcept = default;
	MemoryPool(size_t slotSize = 0, size_t initialBlockSize = 4096);

	~MemoryPool();

	void* allocate(); // ���ٴ���size����ΪslotSize�ǹ̶���
	void deallocate(void* ptr); // ���ٴ���size

	void init(size_t slotSize, size_t initialBlockSize = 4096); // �ṩһ����ʼ������
private:
	void allocateNewBlock(); // �����µĴ��ڴ��

	// ��������������ָ��pΪ�˶��뵽align�ֽ���Ҫ�������ֽ�
	static size_t padPointer(char* p, size_t align);

	// ԭ�Ӳ����Ŀ�������
	// ��һ�����вۣ�slot��ԭ�ӵ������������ͷ����
	bool pushFreeList(Slot* slot);
	// ԭ�ӵص�����������ͷ���ڵ㡣
	Slot* popFreeList();

	Slot* pHeadBlock_;     //ָ���һ�����ڴ���ָ��
	Slot* curSlot_;        // ָ��ǰ���ڴ������һ���ɷ���Ĳ� (ԭ pCurrentSlot_)
	Slot* lastSlot_;       // ָ��ǰ���ڴ�������һ���ɷ���۵Ľ����߽� (ԭ pLastSlot_)

	// Slot* pFreeList_; // <<< �Ƴ��ɵķ�ԭ������ͷ
	//���߳̿���ͬʱ�޸� atomicFreeList_���� pushFreeList �� popFreeList����
	std::atomic<Slot*> atomicFreeList_; // ������ԭ�����͵Ŀ�������ͷ

	size_t slotSize_;      // ÿ�����䵥Ԫ���ۣ��Ĵ�С
	size_t blockSize_;     // ÿ�����ڴ��Ĵ�С

	std::mutex mutexForBlock_; //���������ڱ�������������ָ��
};

// =====================================================================
// HashBucket ��Ķ��� (���Ǳ��������������)
// =====================================================================
class HashBucket {
public:
	// ��ʼ������ MEMORY_POOL_NUM �� MemoryPool �ľ�̬����
	static void initMemoryPool();

	// ��������Ĵ�С�������ڴ�
	static void* useMemory(size_t size);

	// ���ݴ���ָ���ԭʼ��С�������ڴ�
	static void freeMemory(void* ptr, size_t size);

private:
	// ��ȡָ�������� MemoryPool ʵ��������
	static MemoryPool& getMemoryPool(int index);
};

// =====================================================================
// newElement / deleteElement ģ�庯������ (���Ǳ��������������)
// =====================================================================
template<typename T, typename... Args>
T* newElement(Args&&... args) {
	T* p = nullptr;
	// 1. ʹ�� HashBucket ����ԭʼ�ڴ�
	if ((p = reinterpret_cast<T*>(HashBucket::useMemory(sizeof(T)))) != nullptr) {
		// 2. �ڷ�����ڴ���ʹ�� placement new �������
		new(p) T(std::forward<Args>(args)...);
	}
	return p;
}

template<typename T>
void deleteElement(T* p) {
	if (p) {
		// 1. ��ʽ���ö������������
		p->~T();
		// 2. ʹ�� HashBucket �����ڴ�
		HashBucket::freeMemory(reinterpret_cast<void*>(p), sizeof(T));
	}
}

} // namespace Karl_memoryPool