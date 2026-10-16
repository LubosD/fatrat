#ifndef FATRAT_DEBUGLOG_H
#define FATRAT_DEBUGLOG_H
// Temporary timestamped tracing for the libcurl/poller investigation
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <pthread.h>

inline long long frDbgMs()
{
	static const auto t0 = std::chrono::steady_clock::now();
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
}

#define FRDBG(fmt, ...) do { \
	fprintf(stderr, "[FRDBG %8lld t%04x] " fmt "\n", frDbgMs(), (unsigned)(((unsigned long long)(uintptr_t)pthread_self() >> 4) & 0xffff), ##__VA_ARGS__); \
	fflush(stderr); } while(0)

#endif
