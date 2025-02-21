#include "Common.h"
#include "OS/Program.h"

using namespace BB;

BBRWLockScopeWrite::BBRWLockScopeWrite(BBRWLock a_lock)
{
	lock = a_lock;
	OSAcquireSRWLockRead(&lock);
}
BBRWLockScopeWrite::~BBRWLockScopeWrite()
{
	OSReleaseSRWLockWrite(&lock);
}
