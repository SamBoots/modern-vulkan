#pragma once
#include <intrin.h>

static inline void BBInterlockedIncrement64(volatile long long* a_value)
{
	_InterlockedIncrement64(a_value);
}