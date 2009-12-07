/*
 * Copyright 2008-2009, Ingo Weinhold, ingo_weinhold@gmx.de.
 * Copyright 2003-2007, Axel Dörfler, axeld@pinc-software.de.
 * Distributed under the terms of the MIT License.
 *
 * Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
 * Distributed under the terms of the NewOS License.
 */
#ifndef _KERNEL_VM_VM_CACHE_H
#define _KERNEL_VM_VM_CACHE_H


#include <kernel.h>
#include <vm/vm.h>
#include <vm/vm_types.h>

#include "kernel_debug_config.h"


struct kernel_args;


enum {
	CACHE_TYPE_RAM = 0,
	CACHE_TYPE_VNODE,
	CACHE_TYPE_DEVICE,
	CACHE_TYPE_NULL
};

struct VMCachePagesTreeDefinition {
	typedef page_num_t KeyType;
	typedef	vm_page NodeType;

	static page_num_t GetKey(const NodeType* node)
	{
		return node->cache_offset;
	}

	static SplayTreeLink<NodeType>* GetLink(NodeType* node)
	{
		return &node->cache_link;
	}

	static int Compare(page_num_t key, const NodeType* node)
	{
		return key == node->cache_offset ? 0
			: (key < node->cache_offset ? -1 : 1);
	}

	static NodeType** GetListLink(NodeType* node)
	{
		return &node->cache_next;
	}
};

typedef IteratableSplayTree<VMCachePagesTreeDefinition> VMCachePagesTree;

struct VMCache {
public:
								VMCache();
	virtual						~VMCache();

			status_t			Init(uint32 cacheType);

	virtual	void				Delete();

			bool				Lock()
									{ return mutex_lock(&fLock) == B_OK; }
			bool				TryLock()
									{ return mutex_trylock(&fLock) == B_OK; }
			bool				SwitchLock(mutex* from)
									{ return mutex_switch_lock(from, &fLock)
											== B_OK; }
			void				Unlock();
			void				AssertLocked()
									{ ASSERT_LOCKED_MUTEX(&fLock); }

			void				AcquireRefLocked();
			void				AcquireRef();
			void				ReleaseRefLocked();
			void				ReleaseRef();
			void				ReleaseRefAndUnlock()
									{ ReleaseRefLocked(); Unlock(); }

			vm_page*			LookupPage(off_t offset);
			void				InsertPage(vm_page* page, off_t offset);
			void				RemovePage(vm_page* page);

			void				AddConsumer(VMCache* consumer);

			status_t			InsertAreaLocked(VMArea* area);
			status_t			RemoveArea(VMArea* area);

			status_t			WriteModified();
			status_t			SetMinimalCommitment(off_t commitment);
			status_t			Resize(off_t newSize);

			status_t			FlushAndRemoveAllPages();

			// for debugging only
			mutex*				GetLock()
									{ return &fLock; }
			int32				RefCount() const
									{ return fRefCount; }

	// backing store operations
	virtual	status_t			Commit(off_t size);
	virtual	bool				HasPage(off_t offset);

	virtual	status_t			Read(off_t offset, const iovec *vecs,
									size_t count,uint32 flags,
									size_t *_numBytes);
	virtual	status_t			Write(off_t offset, const iovec *vecs, size_t count,
									uint32 flags, size_t *_numBytes);
	virtual	status_t			WriteAsync(off_t offset, const iovec* vecs,
									size_t count, size_t numBytes, uint32 flags,
									AsyncIOCallback* callback);
	virtual	bool				CanWritePage(off_t offset);

	virtual	int32				MaxPagesPerWrite() const
									{ return -1; } // no restriction
	virtual	int32				MaxPagesPerAsyncWrite() const
									{ return -1; } // no restriction

	virtual	status_t			Fault(struct VMAddressSpace *aspace,
									off_t offset);

	virtual	void				Merge(VMCache* source);

	virtual	status_t			AcquireUnreferencedStoreRef();
	virtual	void				AcquireStoreRef();
	virtual	void				ReleaseStoreRef();

public:
			VMArea*				areas;
			list_link			consumer_link;
			list				consumers;
				// list of caches that use this cache as a source
			VMCachePagesTree	pages;
			VMCache*			source;
			off_t				virtual_base;
			off_t				virtual_end;
			off_t				committed_size;
				// TODO: Remove!
			uint32				page_count;
			uint32				temporary : 1;
			uint32				scan_skip : 1;
			uint32				type : 6;

#if DEBUG_CACHE_LIST
			VMCache*			debug_previous;
			VMCache*			debug_next;
#endif

private:
	inline	bool				_IsMergeable() const;

			void				_MergeWithOnlyConsumer();
			void				_RemoveConsumer(VMCache* consumer);

private:
			int32				fRefCount;
			mutex				fLock;
};


#if DEBUG_CACHE_LIST
extern VMCache* gDebugCacheList;
#endif


class VMCacheFactory {
public:
	static	status_t		CreateAnonymousCache(VMCache*& cache,
								bool canOvercommit, int32 numPrecommittedPages,
								int32 numGuardPages, bool swappable);
	static	status_t		CreateVnodeCache(VMCache*& cache,
								struct vnode* vnode);
	static	status_t		CreateDeviceCache(VMCache*& cache,
								addr_t baseAddress);
	static	status_t		CreateNullCache(VMCache*& cache);
};


#ifdef __cplusplus
extern "C" {
#endif

status_t vm_cache_init(struct kernel_args *args);
struct VMCache *vm_cache_acquire_locked_page_cache(struct vm_page *page,
	bool dontWait);

#ifdef __cplusplus
}
#endif


#endif	/* _KERNEL_VM_VM_CACHE_H */
