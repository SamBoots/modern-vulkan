#include "Common.h"
#include "OS/Program.h"

using namespace BB;

BBRWLockScopeWrite::BBRWLockScopeWrite(BBRWLock a_lock)
{
	lock = a_lock;
	OSAcquireSRWLockWrite(&lock);
}
BBRWLockScopeWrite::~BBRWLockScopeWrite()
{
	OSReleaseSRWLockWrite(&lock);
}
