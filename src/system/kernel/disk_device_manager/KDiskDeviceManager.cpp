/*
 * Copyright 2004-2007, Haiku, Inc. All rights reserved.
 * Copyright 2003-2004, Ingo Weinhold, bonefish@cs.tu-berlin.de. All rights reserved.
 *
 * Distributed under the terms of the MIT License.
 */


#include "KDiskDevice.h"
#include "KDiskDeviceManager.h"
#include "KDiskDeviceUtils.h"
#include "KDiskSystem.h"
#include "KFileDiskDevice.h"
#include "KFileSystem.h"
#include "KPartition.h"
#include "KPartitioningSystem.h"
#include "KPartitionVisitor.h"
#include "KPath.h"

#include <VectorMap.h>
#include <VectorSet.h>

#include <KernelExport.h>
#include <util/kernel_cpp.h>

#include <NodeMonitor.h>
#include <node_monitor.h>
#include <Notifications.h>
#include <vfs.h>

#include <dirent.h>
#include <errno.h>
#include <module.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// debugging
//#define DBG(x)
#define DBG(x) x
#define OUT dprintf


// directories for partitioning and file system modules
static const char *kPartitioningSystemPrefix = "partitioning_systems";
static const char *kFileSystemPrefix = "file_systems";


// singleton instance
KDiskDeviceManager *KDiskDeviceManager::sDefaultManager = NULL;


// GetPartitionID
struct GetPartitionID {
	inline partition_id operator()(const KPartition *partition) const
	{
		return partition->ID();
	}
};

// GetDiskSystemID
struct GetDiskSystemID {
	inline disk_system_id operator()(const KDiskSystem *system) const
	{
		return system->ID();
	}
};


// PartitionMap
struct KDiskDeviceManager::PartitionMap : VectorMap<partition_id, KPartition*,
	VectorMapEntryStrategy::ImplicitKey<partition_id, KPartition*,
		GetPartitionID> > {
};
	
// DeviceMap
struct KDiskDeviceManager::DeviceMap : VectorMap<partition_id, KDiskDevice*,
	VectorMapEntryStrategy::ImplicitKey<partition_id, KDiskDevice*,
		GetPartitionID> > {
};
	
// DiskSystemMap
struct KDiskDeviceManager::DiskSystemMap : VectorMap<disk_system_id,
	KDiskSystem*,
	VectorMapEntryStrategy::ImplicitKey<disk_system_id, KDiskSystem*,
		GetDiskSystemID> > {
};
	
// PartitionSet
struct KDiskDeviceManager::PartitionSet : VectorSet<KPartition*> {
};


// DeviceWatcher
class KDiskDeviceManager::DeviceWatcher : public NotificationListener {
	public:
		DeviceWatcher(KDiskDeviceManager *manager)
			:	fManager(manager)
		{
		}

		virtual ~DeviceWatcher()
		{
		}

		virtual void EventOccured(NotificationService &service,
			const KMessage *event)
		{
			int32 opCode = event->GetInt32("opcode", -1);
			switch (opCode) {
				case B_ENTRY_CREATED:
				case B_ENTRY_REMOVED:
				{
					const char *name = event->GetString("name", "");
					dev_t device = event->GetInt32("device", -1);
					ino_t directory = event->GetInt64("directory", -1);
					ino_t node = event->GetInt64("node", -1);

					struct stat st;
					if (vfs_stat_node_ref(device, node, &st) != 0)
						break;

					if (S_ISDIR(st.st_mode)) {
						if (opCode == B_ENTRY_CREATED)
							add_node_listener(device, node, B_WATCH_DIRECTORY,
								*this);
						else
							remove_node_listener(device, node, *this);
					} else {
						if (strcmp(name, "raw") == 0) {
							// a new raw device was added/removed
							KPath path(B_PATH_NAME_LENGTH + 1);
							if (path.InitCheck() != B_OK
								|| vfs_entry_ref_to_path(device, directory,
								name, path.LockBuffer(),
								path.BufferSize()) != B_OK) {
								break;
							}
							path.UnlockBuffer();

							if (opCode == B_ENTRY_CREATED)
								fManager->CreateDevice(path.Path());
							else
								fManager->DeleteDevice(path.Path());
						}
					}

					break;
				}

				default:
					break;
			}
		}

	private:
		KDiskDeviceManager *fManager;
};


static bool
is_active_job_status(uint32 status)
{
	return (status == B_DISK_DEVICE_JOB_SCHEDULED
			|| status == B_DISK_DEVICE_JOB_IN_PROGRESS);
}


//	#pragma mark -


KDiskDeviceManager::KDiskDeviceManager()
	: fLock("disk device manager"),
	  fDevices(new(nothrow) DeviceMap),
	  fPartitions(new(nothrow) PartitionMap),
	  fDiskSystems(new(nothrow) DiskSystemMap),
	  fObsoletePartitions(new(nothrow) PartitionSet),
	  fMediaChecker(-1),
	  fTerminating(false),
	  fDeviceWatcher(new(nothrow) DeviceWatcher(this))
{
	if (InitCheck() != B_OK)
		return;

	RescanDiskSystems();

	fMediaChecker = spawn_kernel_thread(_CheckMediaStatusDaemon,
		"media checker", B_NORMAL_PRIORITY, this);
	if (fMediaChecker >= 0)
		resume_thread(fMediaChecker);

	DBG(OUT("number of disk systems: %ld\n", CountDiskSystems()));
	// TODO: Watch the disk systems and the relevant directories.
}


KDiskDeviceManager::~KDiskDeviceManager()
{
	fTerminating = true;

	status_t result;
	wait_for_thread(fMediaChecker, &result);

	// stop all node monitoring
	_AddRemoveMonitoring("/dev/disk", false);
	delete fDeviceWatcher;

	// remove all devices
	for (int32 cookie = 0; KDiskDevice *device = NextDevice(&cookie);) {
		PartitionRegistrar _(device);
		_RemoveDevice(device);
	}
	// some sanity checks
	if (fPartitions->Count() > 0) {
		DBG(OUT("WARNING: There are still %ld unremoved partitions!\n",
				fPartitions->Count()));
		for (PartitionMap::Iterator it = fPartitions->Begin();
			 it != fPartitions->End(); ++it) {
			DBG(OUT("         partition: %ld\n", it->Value()->ID()));
		}
	}
	if (fObsoletePartitions->Count() > 0) {
		DBG(OUT("WARNING: There are still %ld obsolete partitions!\n",
				fObsoletePartitions->Count()));
		for (PartitionSet::Iterator it = fObsoletePartitions->Begin();
			 it != fObsoletePartitions->End(); ++it) {
			DBG(OUT("         partition: %ld\n", (*it)->ID()));
		}
	}
	// remove all disk systems
	for (int32 cookie = 0;
		 KDiskSystem *diskSystem = NextDiskSystem(&cookie); ) {
		fDiskSystems->Remove(diskSystem->ID());
		if (diskSystem->IsLoaded()) {
			DBG(OUT("WARNING: Disk system `%s' (%ld) is still loaded!\n",
					diskSystem->Name(), diskSystem->ID()));
		} else
			delete diskSystem;
	}

	// delete the containers
	delete fPartitions;
	delete fDevices;
	delete fDiskSystems;
	delete fObsoletePartitions;
}

// InitCheck
status_t
KDiskDeviceManager::InitCheck() const
{
	if (!fPartitions || !fDevices || !fDiskSystems || !fObsoletePartitions)
		return B_NO_MEMORY;

	return (fLock.Sem() >= 0 ? B_OK : fLock.Sem());
}


/** This creates the system's default DiskDeviceManager.
 *	The creation is not thread-safe, and shouldn't be done
 *	more than once.
 */

status_t
KDiskDeviceManager::CreateDefault()
{
	if (sDefaultManager != NULL)
		return B_OK;

	sDefaultManager = new(nothrow) KDiskDeviceManager;
	if (sDefaultManager == NULL)
		return B_NO_MEMORY;

	return sDefaultManager->InitCheck();
}


/**	This deletes the default DiskDeviceManager. The
 *	deletion is not thread-safe either, you should
 *	make sure that it's called only once.
 */

void
KDiskDeviceManager::DeleteDefault()
{
	delete sDefaultManager;
	sDefaultManager = NULL;
}

// Default
KDiskDeviceManager *
KDiskDeviceManager::Default()
{
	return sDefaultManager;
}

// Lock
bool
KDiskDeviceManager::Lock()
{
	return fLock.Lock();
}

// Unlock
void
KDiskDeviceManager::Unlock()
{
	fLock.Unlock();
}

// FindDevice
KDiskDevice *
KDiskDeviceManager::FindDevice(const char *path)
{
	for (int32 cookie = 0; KDiskDevice *device = NextDevice(&cookie); ) {
		if (device->Path() && !strcmp(path, device->Path()))
			return device;
	}
	return NULL;
}

// FindDevice
KDiskDevice *
KDiskDeviceManager::FindDevice(partition_id id, bool deviceOnly)
{
	if (KPartition *partition = FindPartition(id)) {
		KDiskDevice *device = partition->Device();
		if (!deviceOnly || id == device->ID())
			return device;
	}
	return NULL;
}

// FindPartition
KPartition *
KDiskDeviceManager::FindPartition(const char *path)
{
// TODO: Optimize!
	KPath partitionPath;
	if (partitionPath.InitCheck() != B_OK)
		return NULL;

	for (PartitionMap::Iterator it = fPartitions->Begin();
		 it != fPartitions->End();
		 ++it) {
		KPartition *partition = it->Value();
		if (partition->GetPath(&partitionPath) == B_OK
			&& partitionPath == path) {
			return partition;
		}
	}

	return NULL;
}

// FindPartition
KPartition *
KDiskDeviceManager::FindPartition(partition_id id)
{
	PartitionMap::Iterator it = fPartitions->Find(id);
	if (it != fPartitions->End())
		return it->Value();

	return NULL;
}

// FindFileDevice
KFileDiskDevice *
KDiskDeviceManager::FindFileDevice(const char *filePath)
{
	for (int32 cookie = 0; KDiskDevice *device = NextDevice(&cookie); ) {
		KFileDiskDevice *fileDevice = dynamic_cast<KFileDiskDevice*>(device);
		if (fileDevice && fileDevice->FilePath()
			&& !strcmp(filePath, fileDevice->FilePath())) {
			return fileDevice;
		}
	}
	return NULL;
}

// RegisterDevice
KDiskDevice *
KDiskDeviceManager::RegisterDevice(const char *path)
{
	if (ManagerLocker locker = this) {
		if (KDiskDevice *device = FindDevice(path)) {
			device->Register();
			return device;
		}
	}
	return NULL;
}

// RegisterDevice
KDiskDevice *
KDiskDeviceManager::RegisterDevice(partition_id id, bool deviceOnly)
{
	if (ManagerLocker locker = this) {
		if (KDiskDevice *device = FindDevice(id, deviceOnly)) {
			device->Register();
			return device;
		}
	}
	return NULL;
}

// RegisterNextDevice
KDiskDevice *
KDiskDeviceManager::RegisterNextDevice(int32 *cookie)
{
	if (!cookie)
		return NULL;
	if (ManagerLocker locker = this) {
		if (KDiskDevice *device = NextDevice(cookie)) {
			device->Register();
			return device;
		}
	}
	return NULL;
}

// RegisterPartition
KPartition *
KDiskDeviceManager::RegisterPartition(const char *path)
{
	if (ManagerLocker locker = this) {
		if (KPartition *partition = FindPartition(path)) {
			partition->Register();
			return partition;
		}
	}
	return NULL;
}

// RegisterPartition
KPartition *
KDiskDeviceManager::RegisterPartition(partition_id id)
{
	if (ManagerLocker locker = this) {
		if (KPartition *partition = FindPartition(id)) {
			partition->Register();
			return partition;
		}
	}
	return NULL;
}

// RegisterFileDevice
KFileDiskDevice *
KDiskDeviceManager::RegisterFileDevice(const char *filePath)
{
	if (ManagerLocker locker = this) {
		if (KFileDiskDevice *device = FindFileDevice(filePath)) {
			device->Register();
			return device;
		}
	}
	return NULL;
}

// ReadLockDevice
KDiskDevice *
KDiskDeviceManager::ReadLockDevice(partition_id id, bool deviceOnly)
{
	// register device
	KDiskDevice *device = RegisterDevice(id, deviceOnly);
	if (!device)
		return NULL;
	// lock device
	if (device->ReadLock())
		return device;
	device->Unregister();
	return NULL;
}

// WriteLockDevice
KDiskDevice *
KDiskDeviceManager::WriteLockDevice(partition_id id, bool deviceOnly)
{
	// register device
	KDiskDevice *device = RegisterDevice(id, deviceOnly);
	if (!device)
		return NULL;
	// lock device
	if (device->WriteLock())
		return device;
	device->Unregister();
	return NULL;
}

// ReadLockPartition
KPartition *
KDiskDeviceManager::ReadLockPartition(partition_id id)
{
	// register partition
	KPartition *partition = RegisterPartition(id);
	if (!partition)
		return NULL;
	// get and register the device
	KDiskDevice *device = NULL;
	if (ManagerLocker locker = this) {
		device = partition->Device();
		if (device)
			device->Register();
	}
	// lock the device
	if (device->ReadLock()) {
		// final check, if the partition still belongs to the device
		if (partition->Device() == device)
			return partition;
		device->ReadUnlock();
	}
	// cleanup on failure
	if (device)
		device->Unregister();
	partition->Unregister();
	return NULL;
}

// WriteLockPartition
KPartition *
KDiskDeviceManager::WriteLockPartition(partition_id id)
{
	// register partition
	KPartition *partition = RegisterPartition(id);
	if (!partition)
		return NULL;
	// get and register the device
	KDiskDevice *device = NULL;
	if (ManagerLocker locker = this) {
		device = partition->Device();
		if (device)
			device->Register();
	}
	// lock the device
	if (device->WriteLock()) {
		// final check, if the partition still belongs to the device
		if (partition->Device() == device)
			return partition;
		device->WriteUnlock();
	}
	// cleanup on failure
	if (device)
		device->Unregister();
	partition->Unregister();
	return NULL;
}


// ScanPartition
status_t
KDiskDeviceManager::ScanPartition(KPartition* partition)
{
// TODO: This won't do. Locking the DDM while scanning the partition is not a
// good idea. Even locking the device doesn't feel right. Marking the partition
// busy and passing the disk system a temporary clone of the partition_data
// should work as well.
	if (DeviceWriteLocker deviceLocker = partition->Device()) {
		if (ManagerLocker locker = this)
			return _ScanPartition(partition, false);
	}

	return B_ERROR;
}


partition_id
KDiskDeviceManager::CreateDevice(const char *path, bool *newlyCreated)
{
	if (!path)
		return B_BAD_VALUE;

	status_t error = B_ERROR;
	if (ManagerLocker locker = this) {
		KDiskDevice *device = FindDevice(path);
		if (device != NULL) {
			// we already know this device
			if (newlyCreated)
				*newlyCreated = false;

			return device->ID();
		}

		// create a KDiskDevice for it
		device = new(nothrow) KDiskDevice;
		if (!device)
			return B_NO_MEMORY;

		// initialize and add the device
		error = device->SetTo(path);

		// Note: Here we are allowed to lock a device although already having
		// the manager locked, since it is not yet added to the manager.
		DeviceWriteLocker deviceLocker(device);
		if (error == B_OK && !deviceLocker.IsLocked())
			error = B_ERROR;
		if (error == B_OK && !_AddDevice(device))
			error = B_NO_MEMORY;

		// cleanup on error
		if (error != B_OK) {
			delete device;
			return error;
		}

		if (error == B_OK) {
			// scan for partitions
			_ScanPartition(device, false);
			device->UnmarkBusy(true);

			if (newlyCreated)
				*newlyCreated = true;

			return device->ID();
		}
	}

	return error;
}


status_t
KDiskDeviceManager::DeleteDevice(const char *path)
{
	KDiskDevice *device = FindDevice(path);
	if (device == NULL)
		return B_ENTRY_NOT_FOUND;

	PartitionRegistrar _(device, false);
	if (DeviceWriteLocker locker = device) {
		if (_RemoveDevice(device))
			return B_OK;
	}

	return B_ERROR;
}


// CreateFileDevice
partition_id
KDiskDeviceManager::CreateFileDevice(const char *filePath, bool* newlyCreated)
{
	if (!filePath)
		return B_BAD_VALUE;

	// normalize the file path
	KPath normalizedFilePath;
	status_t error = normalizedFilePath.SetTo(filePath, true);
	if (error != B_OK)
		return error;
	filePath = normalizedFilePath.Path();

	KFileDiskDevice *device = NULL;
	if (ManagerLocker locker = this) {
		// check, if the device does already exist
		if ((device = FindFileDevice(filePath))) {
			if (newlyCreated)
				*newlyCreated = false;

			return device->ID();
		}

		// allocate a KFileDiskDevice
		device = new(nothrow) KFileDiskDevice;
		if (!device)
			return B_NO_MEMORY;

		// initialize and add the device
		error = device->SetTo(filePath);

		// Note: Here we are allowed to lock a device although already having
		// the manager locked, since it is not yet added to the manager.
		DeviceWriteLocker deviceLocker(device);
		if (error == B_OK && !deviceLocker.IsLocked())
			error = B_ERROR;
		if (error == B_OK && !_AddDevice(device))
			error = B_NO_MEMORY;

		// scan device
		if (error == B_OK) {
			_ScanPartition(device, false);
			device->UnmarkBusy(true);

			if (newlyCreated)
				*newlyCreated = true;

			return device->ID();
		}

		// cleanup on failure
		delete device;
	} else
		error = B_ERROR;
	return error;
}

// DeleteFileDevice
status_t
KDiskDeviceManager::DeleteFileDevice(const char *filePath)
{
	if (KFileDiskDevice *device = RegisterFileDevice(filePath)) {
		PartitionRegistrar _(device, true);
		if (DeviceWriteLocker locker = device) {
			if (_RemoveDevice(device))
				return B_OK;
		}
	}
	return B_ERROR;
}

// DeleteFileDevice
status_t
KDiskDeviceManager::DeleteFileDevice(partition_id id)
{
	if (KDiskDevice *device = RegisterDevice(id)) {
		PartitionRegistrar _(device, true);
		if (!dynamic_cast<KFileDiskDevice*>(device) || id != device->ID())
			return B_ENTRY_NOT_FOUND;
		if (DeviceWriteLocker locker = device) {
			if (_RemoveDevice(device))
				return B_OK;
		}
	}
	return B_ERROR;
}

// CountDevices
int32
KDiskDeviceManager::CountDevices()
{
	return fDevices->Count();
}

// NextDevice
KDiskDevice *
KDiskDeviceManager::NextDevice(int32 *cookie)
{
	if (!cookie)
		return NULL;
	DeviceMap::Iterator it = fDevices->FindClose(*cookie, false);
	if (it != fDevices->End()) {
		KDiskDevice *device = it->Value();
		*cookie = device->ID() + 1;
		return device;
	}
	return NULL;
}

// PartitionAdded
bool
KDiskDeviceManager::PartitionAdded(KPartition *partition)
{
	return (partition && fPartitions->Put(partition->ID(), partition) == B_OK);
}

// PartitionRemoved
bool
KDiskDeviceManager::PartitionRemoved(KPartition *partition)
{
	if (partition && partition->PrepareForRemoval()
		&& fPartitions->Remove(partition->ID())) {
		// If adding the partition to the obsolete list fails (due to lack
		// of memory), we can't do anything about it. We will leak memory then.
		fObsoletePartitions->Insert(partition);
		partition->MarkObsolete();
		return true;
	}
	return false;
}

// DeletePartition
bool
KDiskDeviceManager::DeletePartition(KPartition *partition)
{
	if (partition && partition->IsObsolete()
		&& partition->CountReferences() == 0
		&& partition->PrepareForDeletion()
		&& fObsoletePartitions->Remove(partition)) {
		delete partition;
		return true;
	}
	return false;
}


// FindDiskSystem
KDiskSystem *
KDiskDeviceManager::FindDiskSystem(const char *name, bool byPrettyName)
{
	for (int32 cookie = 0;
		 KDiskSystem *diskSystem = NextDiskSystem(&cookie); ) {
		if (byPrettyName) {
			if (strcmp(name, diskSystem->PrettyName()) == 0)
				return diskSystem;
		} else {
			if (strcmp(name, diskSystem->Name()) == 0)
				return diskSystem;
		}
	}
	return NULL;
}

// FindDiskSystem
KDiskSystem *
KDiskDeviceManager::FindDiskSystem(disk_system_id id)
{
	DiskSystemMap::Iterator it = fDiskSystems->Find(id);
	if (it != fDiskSystems->End())
		return it->Value();
	return NULL;
}

// CountDiskSystems
int32
KDiskDeviceManager::CountDiskSystems()
{
	return fDiskSystems->Count();
}

// NextDiskSystem
KDiskSystem *
KDiskDeviceManager::NextDiskSystem(int32 *cookie)
{
	if (!cookie)
		return NULL;
	DiskSystemMap::Iterator it = fDiskSystems->FindClose(*cookie, false);
	if (it != fDiskSystems->End()) {
		KDiskSystem *diskSystem = it->Value();
		*cookie = diskSystem->ID() + 1;
		return diskSystem;
	}
	return NULL;
}

// LoadDiskSystem
KDiskSystem *
KDiskDeviceManager::LoadDiskSystem(const char *name, bool byPrettyName)
{
	KDiskSystem *diskSystem = NULL;
	if (ManagerLocker locker = this) {
		diskSystem = FindDiskSystem(name, byPrettyName);
		if (diskSystem && diskSystem->Load() != B_OK)
			diskSystem = NULL;
	}
	return diskSystem;
}

// LoadDiskSystem
KDiskSystem *
KDiskDeviceManager::LoadDiskSystem(disk_system_id id)
{
	KDiskSystem *diskSystem = NULL;
	if (ManagerLocker locker = this) {
		diskSystem = FindDiskSystem(id);
		if (diskSystem && diskSystem->Load() != B_OK)
			diskSystem = NULL;
	}
	return diskSystem;
}

// LoadNextDiskSystem
KDiskSystem *
KDiskDeviceManager::LoadNextDiskSystem(int32 *cookie)
{
	if (!cookie)
		return NULL;
	if (ManagerLocker locker = this) {
		if (KDiskSystem *diskSystem = NextDiskSystem(cookie)) {
			if (diskSystem->Load() == B_OK) {
				*cookie = diskSystem->ID() + 1;
				return diskSystem;
			}
		}
	}
	return NULL;
}

// InitialDeviceScan
status_t
KDiskDeviceManager::InitialDeviceScan()
{
	status_t error = B_ERROR;

	// scan for devices
	if (ManagerLocker locker = this) {
		error = _Scan("/dev/disk");
		if (error != B_OK)
			return error;
	}

	// scan the devices for partitions
	int32 cookie = 0;
	while (KDiskDevice *device = RegisterNextDevice(&cookie)) {
		PartitionRegistrar _(device, true);
		if (DeviceWriteLocker deviceLocker = device) {
			if (ManagerLocker locker = this) {
				error = _ScanPartition(device, false);
				device->UnmarkBusy(true);
				if (error != B_OK)
					break;
			} else
				return B_ERROR;
		} else
			return B_ERROR;
	}
	return error;
}


status_t
KDiskDeviceManager::StartMonitoring()
{
	// do another scan, this will populate the devfs directories
	InitialDeviceScan();
	// start monitoring all dirs under /dev/disk
	status_t result = _AddRemoveMonitoring("/dev/disk", true);
	return result;
}


// _RescanDiskSystems
status_t
KDiskDeviceManager::_RescanDiskSystems(bool fileSystems)
{
	void *cookie = open_module_list(fileSystems
		? kFileSystemPrefix : kPartitioningSystemPrefix);
	if (cookie == NULL)
		return B_NO_MEMORY;

	while (true) {
		KPath name;
		if (name.InitCheck() != B_OK)
			break;
		size_t nameLength = name.BufferSize();
		if (read_next_module_name(cookie, name.LockBuffer(),
				&nameLength) != B_OK) {
			break;
		}
		name.UnlockBuffer();
	
		if (FindDiskSystem(name.Path()))
			continue;

		if (fileSystems) {
			DBG(OUT("file system: %s\n", name.Path()));
			_AddFileSystem(name.Path());
		} else {
			DBG(OUT("partitioning system: %s\n", name.Path()));
			_AddPartitioningSystem(name.Path());
		}
	}

	close_module_list(cookie);
	return B_OK;
}

// RescanDiskSystems
status_t
KDiskDeviceManager::RescanDiskSystems()
{
	// rescan for partitioning and file systems
	_RescanDiskSystems(false);
	_RescanDiskSystems(true);

	return B_OK;
}

// _AddPartitioningSystem
status_t
KDiskDeviceManager::_AddPartitioningSystem(const char *name)
{
	if (!name)
		return B_BAD_VALUE;
	KDiskSystem *diskSystem = new(nothrow) KPartitioningSystem(name);
	if (!diskSystem)
		return B_NO_MEMORY;
	return _AddDiskSystem(diskSystem);
}

// _AddFileSystem
status_t
KDiskDeviceManager::_AddFileSystem(const char *name)
{
	if (!name)
		return B_BAD_VALUE;

	KDiskSystem *diskSystem = new(nothrow) KFileSystem(name);
	if (!diskSystem)
		return B_NO_MEMORY;

	return _AddDiskSystem(diskSystem);
}

// _AddDiskSystem
status_t
KDiskDeviceManager::_AddDiskSystem(KDiskSystem *diskSystem)
{
	if (!diskSystem)
		return B_BAD_VALUE;
DBG(OUT("KDiskDeviceManager::_AddDiskSystem(%s)\n", diskSystem->Name()));
	status_t error = diskSystem->Init();
if (error != B_OK)
DBG(OUT("  initialization failed: %s\n", strerror(error)));
	if (error == B_OK)
		error = fDiskSystems->Put(diskSystem->ID(), diskSystem);
	if (error != B_OK)
		delete diskSystem;
DBG(OUT("KDiskDeviceManager::_AddDiskSystem() done: %s\n", strerror(error)));
	return error;
}

// _AddDevice
bool
KDiskDeviceManager::_AddDevice(KDiskDevice *device)
{
	if (!device || !PartitionAdded(device))
		return false;
	if (fDevices->Put(device->ID(), device) == B_OK)
		return true;
	PartitionRemoved(device);
	return false;
}

// _RemoveDevice
bool
KDiskDeviceManager::_RemoveDevice(KDiskDevice *device)
{
	return (device && fDevices->Remove(device->ID())
			&& PartitionRemoved(device));
}


#if 0
// _UpdateBusyPartitions
/*!
	The device must be write locked, the manager must be locked.
*/
status_t
KDiskDeviceManager::_UpdateBusyPartitions(KDiskDevice *device)
{
	if (!device)
		return B_BAD_VALUE;
	// mark all partitions un-busy
	struct UnmarkBusyVisitor : KPartitionVisitor {
		virtual bool VisitPre(KPartition *partition)
		{
			partition->ClearFlags(B_PARTITION_BUSY
								  | B_PARTITION_DESCENDANT_BUSY);
			return false;
		}
	} visitor;
	device->VisitEachDescendant(&visitor);
	// Iterate through all job queues and all jobs scheduled or in
	// progress and mark their scope busy.
	for (int32 cookie = 0;
		 KDiskDeviceJobQueue *jobQueue = NextJobQueue(&cookie); ) {
		if (jobQueue->Device() != device)
			continue;
		for (int32 i = jobQueue->ActiveJobIndex();
			 KDiskDeviceJob *job = jobQueue->JobAt(i); i++) {
			if (job->Status() != B_DISK_DEVICE_JOB_IN_PROGRESS
				&& job->Status() != B_DISK_DEVICE_JOB_SCHEDULED) {
				continue;
			}
			KPartition *partition = FindPartition(job->ScopeID());
			if (!partition || partition->Device() != device)
				continue;
			partition->AddFlags(B_PARTITION_BUSY);
		}
	}
	// mark all anscestors of busy partitions descendant busy and all
	// descendants busy
	struct MarkBusyVisitor : KPartitionVisitor {
		virtual bool VisitPre(KPartition *partition)
		{
			// parent busy => child busy
			if (partition->Parent() && partition->Parent()->IsBusy())
				partition->AddFlags(B_PARTITION_BUSY);
			return false;
		}

		virtual bool VisitPost(KPartition *partition)
		{
			// child [descendant] busy => parent descendant busy
			if ((partition->IsBusy() || partition->IsDescendantBusy())
				&& partition->Parent()) {
				partition->Parent()->AddFlags(B_PARTITION_DESCENDANT_BUSY);
			}
			return false;
		}
	} visitor2;
	device->VisitEachDescendant(&visitor2);
	return B_OK;
}
#endif


// _Scan
status_t
KDiskDeviceManager::_Scan(const char *path)
{
DBG(OUT("KDiskDeviceManager::_Scan(%s)\n", path));
	status_t error = B_ENTRY_NOT_FOUND;
	struct stat st;
	if (lstat(path, &st) < 0) {
		return errno;
	}
	if (S_ISDIR(st.st_mode)) {
		// a directory: iterate through its contents
		DIR *dir = opendir(path);
		if (!dir)
			return errno;
		while (dirent *entry = readdir(dir)) {
			// skip "." and ".."
			if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
				continue;
			KPath entryPath;
			if (entryPath.SetPath(path) != B_OK
				|| entryPath.Append(entry->d_name) != B_OK) {
				continue;
			}
			if (_Scan(entryPath.Path()) == B_OK)
				error = B_OK;
		}
		closedir(dir);
	} else {
		// not a directory
		// check, if it is named "raw"
		int32 len = strlen(path);
		int32 leafLen = strlen("/raw");
		if (len <= leafLen || strcmp(path + len - leafLen, "/raw"))
			return B_ERROR;
		if (FindDevice(path) != NULL) {
			// we already know this device
			return B_OK;
		}

DBG(OUT("  found device: %s\n", path));
		// create a KDiskDevice for it
		KDiskDevice *device = new(nothrow) KDiskDevice;
		if (!device)
			return B_NO_MEMORY;
		// init the KDiskDevice
		error = device->SetTo(path);
		// add the device
		if (error == B_OK && !_AddDevice(device))
			error = B_NO_MEMORY;
		// cleanup on error
		if (error != B_OK)
			delete device;
	}
	return error;
}

// _ScanPartition
/*!
	The device must be write locked, the manager must be locked.
*/
status_t
KDiskDeviceManager::_ScanPartition(KPartition *partition, bool async)
{
// TODO: There's no reason why the manager needs to be locked anymore.
	if (!partition)
		return B_BAD_VALUE;

// TODO: Reimplement asynchronous scanning, if we really need it.
#if 0
	if (async) {
		// create a new job queue for the device
		KDiskDeviceJobQueue *jobQueue = new(nothrow) KDiskDeviceJobQueue;
		if (!jobQueue)
			return B_NO_MEMORY;
		jobQueue->SetDevice(partition->Device());

		// create a job for scanning the device and add it to the job queue
		KDiskDeviceJob *job = fJobFactory->CreateScanPartitionJob(partition->ID());
		if (!job) {
			delete jobQueue;
			return B_NO_MEMORY;
		}

		if (!jobQueue->AddJob(job)) {
			delete jobQueue;
			delete job;
			return B_NO_MEMORY;
		}

		// add the job queue
		status_t error = AddJobQueue(jobQueue);
		if (error != B_OK)
			delete jobQueue;

		return error;
	}
#endif

	// scan synchronously

	return _ScanPartition(partition);
}


status_t
KDiskDeviceManager::_ScanPartition(KPartition *partition)
{
	// the partition's device must be write-locked
	if (!partition)
		return B_BAD_VALUE;
	if (!partition->Device()->HasMedia())
		return B_OK;
	if (partition->DiskSystem() != NULL) {
		// TODO: this is more or less a hack to allow rescanning a partition
		for (int32 i = 0; KPartition *child = partition->ChildAt(i); i++)
			_ScanPartition(child);
		return B_OK;
	}

	DBG(
		KPath partitionPath;
		partition->GetPath(&partitionPath);
		OUT("KDiskDeviceManager::_ScanPartition(%s)\n", partitionPath.Path());
	)

	// publish the partition
	status_t error = B_OK;
	if (!partition->IsPublished()) {
		error = partition->PublishDevice();
		if (error != B_OK)
			return error;
	}

	// find the disk system that returns the best priority for this partition
	float bestPriority = -1;
	KDiskSystem *bestDiskSystem = NULL;
	void *bestCookie = NULL;
	int32 itCookie = 0;
	while (KDiskSystem *diskSystem = LoadNextDiskSystem(&itCookie)) {
		DBG(OUT("  trying: %s\n", diskSystem->Name()));

		void *cookie = NULL;
		float priority = diskSystem->Identify(partition, &cookie);

		DBG(OUT("  returned: %ld/1000\n", int32(1000 * priority)));

		if (priority >= 0 && priority > bestPriority) {
			// new best disk system
			if (bestDiskSystem) {
				bestDiskSystem->FreeIdentifyCookie(partition, bestCookie);
				bestDiskSystem->Unload();
			}
			bestPriority = priority;
			bestDiskSystem = diskSystem;
			bestCookie = cookie;
		} else {
			// disk system doesn't identify the partition or worse than our
			// current favorite
			if (priority >= 0)
				diskSystem->FreeIdentifyCookie(partition, cookie);
			diskSystem->Unload();
		}
	}

	// now, if we have found a disk system, let it scan the partition
	if (bestDiskSystem) {
		DBG(OUT("  scanning with: %s\n", bestDiskSystem->Name()));
		error = bestDiskSystem->Scan(partition, bestCookie);
		bestDiskSystem->FreeIdentifyCookie(partition, bestCookie);
		if (error == B_OK) {
			partition->SetDiskSystem(bestDiskSystem);
			for (int32 i = 0; KPartition *child = partition->ChildAt(i); i++)
				_ScanPartition(child);
		} else {
			// TODO: Handle the error.
			DBG(OUT("  scanning failed: %s\n", strerror(error)));
		}

		// now we can safely unload the disk system -- it has been loaded by
		// the partition(s) and thus will not really be unloaded
		bestDiskSystem->Unload();
	} else {
		// contents not recognized
		// nothing to be done -- partitions are created as unrecognized
	}

	return error;
}


status_t
KDiskDeviceManager::_AddRemoveMonitoring(const char *path, bool add)
{
	struct stat st;
	if (lstat(path, &st) < 0)
		return errno;

	status_t error = B_ENTRY_NOT_FOUND;
	if (S_ISDIR(st.st_mode)) {
		if (add) {
			error = add_node_listener(st.st_dev, st.st_ino, B_WATCH_DIRECTORY,
				*fDeviceWatcher);
		} else {
			error = remove_node_listener(st.st_dev, st.st_ino,
				*fDeviceWatcher);
		}
		if (error != B_OK)
			return error;

		DIR *dir = opendir(path);
		if (!dir)
			return errno;

		while (dirent *entry = readdir(dir)) {
			// skip "." and ".."
			if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
				continue;

			KPath entryPath;
			if (entryPath.SetPath(path) != B_OK
				|| entryPath.Append(entry->d_name) != B_OK) {
				continue;
			}

			if (_AddRemoveMonitoring(entryPath.Path(), add) == B_OK)
				error = B_OK;
		}
		closedir(dir);
	}

	return error;
}


status_t
KDiskDeviceManager::_CheckMediaStatus()
{
	while (!fTerminating) {
		int32 cookie = 0;
		while (KDiskDevice* device = RegisterNextDevice(&cookie)) {
			PartitionRegistrar _(device, true);
			DeviceWriteLocker locker(device);

			if (device->IsBusy(true))
				continue;

			bool hadMedia = device->HasMedia();
			device->UpdateMediaStatusIfNeeded();

			if (!device->MediaChanged() && (device->HasMedia() || !hadMedia))
				continue;

			device->MarkBusy(true);
			device->UninitializeMedia();

			if (device->MediaChanged()) {
				dprintf("Media changed from %s\n", device->Path());
				device->UpdateGeometry();
				_ScanPartition(device, false);
			} else if (!device->HasMedia() && hadMedia) {
				dprintf("Media removed from %s\n", device->Path());
			}

			device->UnmarkBusy(true);
		}

		snooze(1000000);
	}

	return 0;
}


status_t
KDiskDeviceManager::_CheckMediaStatusDaemon(void* self)
{
	return ((KDiskDeviceManager*)self)->_CheckMediaStatus();
}
