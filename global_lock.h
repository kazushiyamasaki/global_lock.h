/*
 * global_lock.h -- a header file providing a single global lock with
 *                  macro-configurable name and scope
 * version 0.9.0, June 12, 2025
 *
 * License: zlib License
 *
 * Copyright (c) 2025 Kazushi Yamasaki
 *
 * This software is provided ‘as-is’, without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 * claim that you wrote the original software. If you use this software
 * in a product, an acknowledgment in the product documentation would be
 * appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source
 * distribution.
 *
 *
 *
 * IMPORTANT:
 * Be sure to properly define the GLOBAL_LOCK_FUNC_NAME,
 * GLOBAL_UNLOCK_FUNC_NAME and GLOBAL_LOCK_FUNC_SCOPE macros before including
 * this header file. GLOBAL_LOCK_FUNC_SCOPE only accepts 'static' or blank.
 * 
 * Additionally, you must call global_lock_quit(); when the program
 * terminates.
 */

#ifndef GLOBAL_LOCK_FUNC_NAME
#define GLOBAL_LOCK_FUNC_NAME global_lock_lock
#endif

#ifndef GLOBAL_UNLOCK_FUNC_NAME
#define GLOBAL_UNLOCK_FUNC_NAME global_lock_unlock
#endif

#ifndef GLOBAL_LOCK_FUNC_SCOPE
#define GLOBAL_LOCK_FUNC_SCOPE
#endif


#include <stdio.h>
#include <stdlib.h>


#ifdef __GNUC__
	#define LIKELY(x)   __builtin_expect(!!(x), 1)
	#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
	#define LIKELY(x)   (x)
	#define UNLIKELY(x) (x)
#endif


#if defined (_WIN32) && (!defined(_WIN32_WINNT) || (_WIN32_WINNT < 0x0600))
	#error "This program requires Windows Vista or later. Define _WIN32_WINNT accordingly."
#endif


#if defined (__unix__) || defined (__linux__) || defined (__APPLE__)
	#include <unistd.h>
#endif


#if defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L) && !defined (__STDC_NO_THREADS__)
	#define C11_THREADS_AVAILABLE

	#include <threads.h>
	static mtx_t global_lock_lock_mutex;
	static once_flag mtx_init_once = ONCE_FLAG_INIT;

	static void init_mtx (void) {
		if (UNLIKELY(mtx_init(&global_lock_lock_mutex, mtx_plain) != thrd_success)) {
			fprintf(stderr, "Failed to initialize the mutex!\nFile: %s   Line: %d\n", __FILE__, __LINE__);
			exit(EXIT_FAILURE);
		}
	}
#elif defined (_POSIX_THREADS) && (_POSIX_THREADS > 0)
	#define PTHREAD_AVAILABLE

	#include <pthread.h>
	static pthread_mutex_t global_lock_lock_mutex = PTHREAD_MUTEX_INITIALIZER;
#elif defined (_WIN32)
	#include <windows.h>
	static INIT_ONCE cs_init_once = INIT_ONCE_STATIC_INIT;
	static CRITICAL_SECTION global_lock_lock_cs;

	static BOOL CALLBACK InitCriticalSection (PINIT_ONCE InitOnce, PVOID Parameter, PVOID *Context) {
		(void)InitOnce;  (void)Parameter;  (void)Context;
		InitializeCriticalSection(&global_lock_lock_cs);
		return true;
	}

	static bool winver_checked = false;
	static bool is_windows_vista_or_later (void) {
		OSVERSIONINFO osvi = {0};
		osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);  /* 必要な初期化 */
		if (UNLIKELY(!GetVersionEx(&osvi))) {
			return false;
		}
		return (osvi.dwMajorVersion >= 6);
	}
#elif defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L) && !defined (__STDC_NO_ATOMICS__)
	#define STDSTOMIC_AVAILABLE

	#include <stdatomic.h>
	static atomic_flag global_lock_lock_flag = ATOMIC_FLAG_INIT;
#elif defined (__GNUC__)  /* GCC または Clang */
	#ifdef __has_builtin  /* Clang 3以降 */
		#if __has_builtin (__atomic_exchange_n)
			#define GCC_ATOMIC_BUILTIN_AVAILABLE
		#elif __has_builtin (__sync_lock_test_and_set)
			#define GCC_SYNC_BUILTIN_AVAILABLE
		#endif
	#elif defined (__GNUC_MINOR__)  /* GCC or old Clang */
		#if (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7))
			#define GCC_ATOMIC_BUILTIN_AVAILABLE
		#elif !defined (__clang__) && (__GNUC__ == 4 && __GNUC_MINOR__ >= 1)
			#define GCC_SYNC_BUILTIN_AVAILABLE
		#endif
	#endif
	#if defined (GCC_ATOMIC_BUILTIN_AVAILABLE) || defined (GCC_SYNC_BUILTIN_AVAILABLE)
		static int global_lock_lock_int = 0;
	#else
		#error "No valid locking mechanism found on this platform."
	#endif
#else
	#error "No valid locking mechanism found on this platform."
#endif


#if !defined (C11_THREADS_AVAILABLE) && !defined (PTHREAD_AVAILABLE) && !defined (_WIN32)
	#if (defined (__x86_64__) || defined (__amd64__) || defined (_M_X64) || defined (__i386__) || defined (_M_IX86))
		#include <emmintrin.h>
		#define SPIN_WAIT() _mm_pause()
	#elif defined (_POSIX_PRIORITY_SCHEDULING)
		#include <sched.h>
		#define SPIN_WAIT() sched_yield()
	#else
		#define SPIN_WAIT() do { volatile size_t i; for (i = 0; i < 1000; ++i) { __asm__ __volatile__ ("" ::: "memory"); } } while (0)
	#endif
#endif


GLOBAL_LOCK_FUNC_SCOPE void GLOBAL_LOCK_FUNC_NAME (void) {
#ifdef C11_THREADS_AVAILABLE
	call_once(&mtx_init_once, init_mtx);

	mtx_lock(&global_lock_lock_mutex);
#elif defined (PTHREAD_AVAILABLE)
	pthread_mutex_lock(&global_lock_lock_mutex);
#elif defined (_WIN32)
	if (UNLIKELY(winver_checked == false)) {
		if (!is_windows_vista_or_later())
			exit(EXIT_FAILURE);
		else
			winver_checked = true;
	}  /* なるべく実行回数を減らしたいだけなので、複数回実行されても問題はない */

	InitOnceExecuteOnce(&cs_init_once, InitCriticalSection, NULL, NULL);

	EnterCriticalSection(&global_lock_lock_cs);
#elif defined (STDSTOMIC_AVAILABLE)
	while (atomic_flag_test_and_set_explicit(&global_lock_lock_flag, memory_order_acquire)) {
		SPIN_WAIT();
	}
#elif defined (GCC_ATOMIC_BUILTIN_AVAILABLE)
	while (__atomic_exchange_n(&global_lock_lock_int, 1, __ATOMIC_SEQ_CST)) {
		SPIN_WAIT();
	}
#elif defined (GCC_SYNC_BUILTIN_AVAILABLE)
	while (__sync_lock_test_and_set(&global_lock_lock_int, 1)) {
		SPIN_WAIT();
	}
#endif
}


GLOBAL_LOCK_FUNC_SCOPE void GLOBAL_UNLOCK_FUNC_NAME (void) {
#ifdef C11_THREADS_AVAILABLE
	mtx_unlock(&global_lock_lock_mutex);
#elif defined (PTHREAD_AVAILABLE)
	pthread_mutex_unlock(&global_lock_lock_mutex);
#elif defined (_WIN32)
	LeaveCriticalSection(&global_lock_lock_cs);
#elif defined (STDSTOMIC_AVAILABLE)
	atomic_flag_clear_explicit(&global_lock_lock_flag, memory_order_release);
#elif defined (GCC_ATOMIC_BUILTIN_AVAILABLE)
	__atomic_store_n(&global_lock_lock_int, 0, __ATOMIC_SEQ_CST);
#elif defined (GCC_SYNC_BUILTIN_AVAILABLE)
	__sync_lock_release(&global_lock_lock_int);
#endif
}


static void global_lock_quit (void) {
#ifdef C11_THREADS_AVAILABLE
	mtx_destroy(&global_lock_lock_mutex);
#elif defined (PTHREAD_AVAILABLE)
	pthread_mutex_destroy(&global_lock_lock_mutex);
#elif defined (_WIN32)
	DeleteCriticalSection(&global_lock_lock_cs);
#endif
}
