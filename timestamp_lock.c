#if defined(_WIN32)
#define WIN32_MEAN_AND_LEAN
#include <windows.h>
#endif

#include "timestamp_lock.h"

static u64 timestamp_utc(void)
{
#if defined(__linux__)
	struct timespec ts;
	int result = syscall(SYS_clock_gettime, CLOCK_REALTIME, &ts);
	if (result)
		return 0;
	return ts.tv_sec;
#elif defined(_WIN32)
	FILETIME ft;
	GetSystemTimeAsFileTime(&ft);

	ULARGE_INTEGER uli;
	uli.LowPart = ft.dwLowDateTime;
	uli.HighPart = ft.dwHighDateTime;
			
	// Convert Windows file time (100ns since 1601-01-01) to 
	// Unix epoch time (seconds since 1970-01-01)
	// 116444736000000000 = number of 100ns intervals from 1601 to 1970
	return (uli.QuadPart - 116444736000000000ULL) / 10000000ULL;
#else
	return 0;
#endif
}

static u64 atomic_load(volatile u64 *ptr)
{
#if COMPILER_MSVC
	return *ptr; // TODO: Is this read atomic?
#elif COMPILER_GCC || COMPILER_CLANG
	return __atomic_load_n(ptr, __ATOMIC_RELAXED);
#endif
}

static void atomic_store(volatile u64 *ptr, u64 val)
{
#if COMPILER_MSVC
	_InterlockedExchange64((volatile s64*) ptr, 0);
#elif COMPILER_GCC || COMPILER_CLANG
	__atomic_store_n(ptr, 0, __ATOMIC_RELEASE);
#endif
}

static int atomic_compare_exchange(volatile u64 *ptr, u64 expect, u64 new_value)
{
#if COMPILER_MSVC
	return (u64) _InterlockedCompareExchange64((volatile s64*) ptr, new_value, expect) == expect;
#elif COMPILER_GCC || COMPILER_CLANG
	return __atomic_compare_exchange_n(ptr, &expect, new_value, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
#endif
}

int timestamp_trylock(volatile u64 *word, u64 *ticket, int timeout_sec, int *crash)
{
	u64 now = timestamp_utc();
	if (now == 0)
		return -EAGAIN;

	u64 old_word = atomic_load(word);
	if (old_word >= now)
		return -EAGAIN; // Lock not expired yet

	u64 new_word = now + timeout_sec;
	if (!atomic_compare_exchange(word, old_word, new_word))
		return -EAGAIN;

	*ticket = new_word;
	*crash = (old_word > 0);
	return 0;
}

int timestamp_lock(volatile u64 *word, u64 *ticket, int timeout_sec, int *crash)
{
	for (;;) {

		int code = trylock(word, ticket, timeout_sec, crash);

		if (code == 0)
			break;

		if (code != -EAGAIN)
			return code;

		// TODO: Wait
	}
	return 0;
}

void timestamp_unlock(volatile u64 *word, u64 *ticket)
{
	atomic_compare_exchange(word, *ticket, 0);

	// TODO: wake up
}

int timestamp_refreshlock(volatile u64 *word, u64 *ticket, int postpone_sec)
{
	u64 now = timestamp_utc();
	if (now == 0)
		return -EAGAIN;

	u64 new_word = now + postpone_sec;
	if (!atomic_compare_exchange(word, *ticket, new_word)) {
		*ticket = 0;
		return -ETIMEDOUT;
	}

	*ticket = now + postpone_sec;
	return 0;
}
