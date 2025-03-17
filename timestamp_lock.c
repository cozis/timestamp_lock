#if defined(_WIN32)
#define WIN32_MEAN_AND_LEAN
#include <windows.h>
#endif

#if defined(__linux__)
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <sys/syscall.h>
#include <linux/futex.h>
#define CLOCK_REALTIME 0
#endif

#include "timestamp_lock.h"

// Returns the current time in seconds since 1 Jan 1970
static u64 timestamp_utc(void)
{
#if defined(__linux__)
	struct timespec ts;
	int result = syscall(SYS_clock_gettime, CLOCK_REALTIME, &ts);
	if (result)
		return 0;
	return ts.tv_sec;
#else
	FILETIME ft;
	GetSystemTimeAsFileTime(&ft);

	ULARGE_INTEGER uli;
	uli.LowPart = ft.dwLowDateTime;
	uli.HighPart = ft.dwHighDateTime;
			
	// Convert Windows file time (100ns since 1601-01-01) to 
	// Unix epoch time (seconds since 1970-01-01)
	// 116444736000000000 = number of 100ns intervals from 1601 to 1970
	return (uli.QuadPart - 116444736000000000ULL) / 10000000ULL;
#endif
}

#if defined(__linux__)
static int futex(unsigned int *uaddr,
	int futex_op, unsigned int val,
	const struct timespec *timeout,
	unsigned int *uaddr2, unsigned int val3)
{
	return syscall(SYS_futex, uaddr, futex_op, val, timeout, uaddr2, val3);
}
#endif

int timestamp_trylock(volatile u64 *word, u64 *ticket, int timeout_sec, int *crash)
{
	u64 now = timestamp_utc();
	if (now == 0)
		return -EAGAIN;

#if defined(_MSC_VER)
		u64 old_word = *word;
#else
		u64 old_word = __atomic_load_n(word, __ATOMIC_ACQUIRE);
#endif
	if (old_word >= now)
		return -EAGAIN; // Lock not expired yet

	u64 new_word = now + timeout_sec;

#if defined(_MSC_VER)
	if ((u64) _InterlockedCompareExchange64((volatile s64*) word, new_word, old_word) != old_word)
		return -EAGAIN;
#else
	if (!__atomic_compare_exchange_n(word, &old_word, new_word, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
		return -EAGAIN;
#endif

	*ticket = new_word;
	*crash = (old_word > 0);

	// If a crash happened, we missed the memory barriers from the unlock operation
	if (*crash) {
#if defined(_MSC_VER)
		_ReadWriteBarrier();
		MemoryBarrier();
#else
		__atomic_thread_fence(__ATOMIC_SEQ_CST);
#endif
	}
	return 0;
}

int timestamp_lock(volatile u64 *word, u64 *ticket, int timeout_sec, int *crash)
{
	for (;;) {

		u64 now = timestamp_utc();

		int code = timestamp_trylock(word, ticket, timeout_sec, crash);

		if (code == 0)
			break;

		if (code != -EAGAIN)
			return code;

#if defined(_MSC_VER)
		u64 old_word = *word;
#else
		u64 old_word = __atomic_load_n(word, __ATOMIC_ACQUIRE);
#endif

		if (old_word > now) {

#if defined(_WIN32)

			if (!WaitOnAddress(
				(volatile VOID*) word,
				(PVOID) &old_word,
				sizeof(u64),
				(old_word - now) * 1000))
				return -EAGAIN;

#elif defined(__linux__)

			struct timespec ts;
			ts.tv_sec = old_word - now;
			ts.tv_nsec = 1000;

			errno = 0;
			long ret = futex((unsigned int*) word, FUTEX_WAIT, (unsigned int) old_word, &ts, NULL, 0);
			if (ret == -1 && errno != EAGAIN && errno != EINTR && errno != ETIMEDOUT)
				return -errno;
#endif
		}
	}
	return 0;
}

int timestamp_unlock(volatile u64 *word, u64 ticket)
{
#if defined(_MSC_VER)
	if ((u64) _InterlockedCompareExchange64((volatile s64*) word, 0, ticket) != ticket)
		return -ETIMEDOUT;
#else
	if (!__atomic_compare_exchange_n(word, &ticket, 0, 0, __ATOMIC_RELEASE, __ATOMIC_RELAXED))
		return -ETIMEDOUT;
#endif

#if defined(_WIN32)
	WakeByAddressAll((PVOID) word);
#elif defined(__linux__)
	errno = 0;
	long ret = futex((unsigned int*) word, FUTEX_WAKE, INT_MAX, NULL, NULL, 0);
	if (ret < 0) return -errno;
#endif

	return 0;
}

int timestamp_refreshlock(volatile u64 *word, u64 *ticket, int postpone_sec)
{
	u64 now = timestamp_utc();
	if (now == 0)
		return -EAGAIN;

	u64 new_word = now + postpone_sec;

#if defined(_MSC_VER)
	if ((u64) _InterlockedCompareExchange64((volatile s64*) word, new_word, *ticket) != *ticket)
		return -ETIMEDOUT;
#else
	u64 dummy = *ticket;
	if (!__atomic_compare_exchange_n(word, &dummy, new_word, 0, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED))
		return -ETIMEDOUT;
#endif

	*ticket = new_word;
	return 0;
}
