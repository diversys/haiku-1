#include "area.h"
#include "olist.h"

class areaManager // One of these per process
{
	private:
		orderedList areas;
		team_id team;
		sem_id myLock;
		static long nextAreaID;
	public:
		areaManager ();
		void addArea(area *newArea) {areas.add(newArea);}
		void removeArea(area *oldArea) {areas.remove(oldArea); }
		void freeArea(area_id area);
		team_id getTeam(void) {return team;}
		unsigned long getNextAddress(int pages,unsigned long minimum=USER_BASE);
		area *findArea(void *address);
		area *findAreaLock(void *address);
		area *findArea(char *address);
		area *findArea(area_id id);
		area *findAreaLock(area_id id);
		status_t setProtection(int areaID,protectType prot)
		{
			status_t retVal;
			lock();
			area *myArea=findArea(areaID);
			if (myArea)
				retVal= myArea->setProtection(prot);
			else
				retVal= B_ERROR;
			unlock();
			return retVal;
		}
		status_t resizeArea(int Area,size_t size)
		{
		status_t retVal;				
		lock();
		area *oldArea=findArea(Area);
		if (oldArea)
			retVal= oldArea->resize(size);
		else
			retVal= B_ERROR; 
		unlock();
		return retVal;
		}
		status_t getAreaInfo(int areaID,area_info *dest)
		{
			status_t retVal;
			lock();
			area *oldArea=findArea(areaID);
			if (oldArea)
				retVal=oldArea->getInfo(dest);
			else
				retVal=B_ERROR;
			unlock();
			return retVal;
		}
		status_t getInfoAfter(int32 & areaID,area_info *dest)
		{
			status_t retVal;
			lock();
			area *oldArea=findArea(areaID);
			if (oldArea->next)
				{
				area *newCurrent=(reinterpret_cast<area *>(oldArea->next));
				retVal=newCurrent->getInfo(dest);
				areaID=(int)newCurrent;
				}
			else
				retVal=B_ERROR;
			unlock();
			return retVal;
		}
		int getAreaByName(char *name)
		{
		int retVal;
		lock();
		area *oldArea=findArea(name);
		if (oldArea)
			retVal= oldArea->getAreaID();
		else
			retVal= B_ERROR; 
		unlock();
		return retVal;
		}
		int createArea(char *AreaName,int pageCount,void **address, addressSpec addType,pageState state,protectType protect) ;
		void pager(int desperation);
		void saver(void);
		void lock() { acquire_sem(myLock); }
		void unlock() {release_sem(myLock);}
		void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset);
		status_t munmap(void *addr,size_t len);
		int cloneArea(int newAreaID,char *AreaName,void **address, addressSpec addType=ANY, pageState state=NO_LOCK, protectType prot=writable);
		
		bool fault(void *fault_address, bool writeError); // true = OK, false = panic.

		char getByte(unsigned long offset); // This is for testing only
		void setByte(unsigned long offset,char value); // This is for testing only
		int getInt(unsigned long offset); // This is for testing only
		void setInt(unsigned long offset,int value); // This is for testing only
};
