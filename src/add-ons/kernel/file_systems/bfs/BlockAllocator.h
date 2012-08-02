/*
 * Copyright 2001-2012, Axel Dörfler, axeld@pinc-software.de.
 * This file may be used under the terms of the MIT License.
 */
#ifndef BLOCK_ALLOCATOR_H
#define BLOCK_ALLOCATOR_H


#include "system_dependencies.h"


class AllocationGroup;
class Inode;
class Transaction;
class Volume;
struct disk_super_block;
struct block_run;


//#define DEBUG_ALLOCATION_GROUPS
//#define DEBUG_FRAGMENTER


class BlockAllocator {
public:
							BlockAllocator(Volume* volume);
							~BlockAllocator();

			status_t		Initialize(bool full = true,
								bool alreadyLocked = false);
			status_t		InitializeAndClearBitmap(Transaction& transaction);
			status_t		Reinitialize();

			void			Uninitialize();

			void			SetRange(off_t beginBlock, off_t endBlock);
			bool			IsBlockRunInRange(block_run run) const;
			bool			IsBlockRunOutsideRange(block_run run) const;

			status_t		AllocateForInode(Transaction& transaction,
								const block_run* parent, mode_t type,
								block_run& run);
			status_t		Allocate(Transaction& transaction, Inode* inode,
								off_t numBlocks, block_run& run,
								uint16 minimum = 1);
			status_t		Free(Transaction& transaction, block_run run);

			status_t		AllocateBlocks(Transaction& transaction,
								int32 group, uint16 start, uint16 numBlocks,
								uint16 minimum, block_run& run,
								bool inode = false);

			status_t		AllocateBlockRun(Transaction& transaction,
								block_run run);

			status_t		CheckBlocks(off_t start, off_t length,
								bool allocated = true,
								off_t* firstError = NULL);
			status_t		CheckBlockRun(block_run run,
								const char* type = NULL,
								bool allocated = true);
			bool			IsValidBlockRun(block_run run);

			recursive_lock&	Lock() { return fLock; }

#ifdef BFS_DEBUGGER_COMMANDS
			void			Dump(int32 index);
#endif
#ifdef DEBUG_FRAGMENTER
			void			Fragment();
#endif

private:
#ifdef DEBUG_ALLOCATION_GROUPS
			void			_CheckGroup(int32 group) const;
#endif
	static	status_t		_Initialize(BlockAllocator* self);

private:
			Volume*			fVolume;
			recursive_lock	fLock;
			AllocationGroup* fGroups;
			int32			fNumGroups;
			uint32			fBlocksPerGroup;
			uint32			fNumBlocks;

			off_t			fBeginBlock;
			off_t			fEndBlock;
};

#ifdef BFS_DEBUGGER_COMMANDS
#if BFS_TRACING
int dump_block_allocator_blocks(int argc, char** argv);
#endif
int dump_block_allocator(int argc, char** argv);
#endif

#endif	// BLOCK_ALLOCATOR_H
