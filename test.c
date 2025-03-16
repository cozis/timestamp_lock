#include "timestamp_lock.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_MEAN_AND_LEAN
#include <windows.h>
typedef HANDLE Thread;
typedef DWORD  TReturn;
#endif

#if defined(__linux__)
#include <pthread.h>
typedef pthread_t Thread;
typedef unsigned long TReturn;
#endif

static Thread spawn_thread(TReturn (*func)(void*), void *arg);
static void   join_thread(Thread thread);

#define ITER_PER_THREAD 5000000

u64 lock = 0;
u64 counter = 0;

TReturn func(void*)
{
	for (int i = 0; i < ITER_PER_THREAD; i++) {
		u64 ticket;
		int crash;
		int code = timestamp_lock(&lock, &ticket, 1, &crash);
		if (code) {
			printf("%s\n", strerror(-code));
			abort();
		}
		counter++;
		code = timestamp_unlock(&lock, ticket);
		if (code) {
			printf("%s\n", strerror(-code));
			abort();
		}
/*
		if (rand() & 31) {
			code = timestamp_unlock(&lock, ticket);
			if (code) {
				printf("%s\n", strerror(-code));
				abort();
			}
		}
*/
	}
	return 0;
}

int main(void)
{
	Thread handles[16];
	for (int i = 0; i < 16; i++)
		handles[i] = spawn_thread(func, NULL);
	for (int i = 0; i < 16; i++)
		join_thread(handles[i]);
	printf("counter=%llu\n", counter);
	printf("expected=%llu\n", (u64) ITER_PER_THREAD * 16);
	return 0;
}

static Thread spawn_thread(TReturn (*func)(void*), void *arg)
{
#if defined(_WIN32)
	HANDLE thread = CreateThread(NULL, 0, func, arg, 0, NULL);
	if (thread == INVALID_HANDLE_VALUE)
		abort();
	return thread;
#else
	pthread_t thread;
	int r = pthread_create(&thread, NULL, (void*) func, arg);
	if (r)
		abort();
	return thread;
#endif
}

static void join_thread(Thread thread)
{
#if defined(_WIN32)
	WaitForSingleObject(thread, INFINITE);
	CloseHandle(thread);
#else
	pthread_join(thread, NULL);
#endif
}
