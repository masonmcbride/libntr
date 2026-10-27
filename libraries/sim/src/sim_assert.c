#include <stdarg.h>
#include <stdio.h>
#include "simulator/assert.h"

#if defined(SDK_BUILD_LINUX) || defined(SDK_BUILD_MACOS)
#include <signal.h>
#endif

#ifdef SDK_BUILD_WIN64
#include "debugapi.h"
#endif

void SIM_handleAssertionFailure(const char* filename, unsigned int lineno, const char* exp) {
	printf("[SIM ASSERTION FAILED] %s:%d\n", filename, lineno);
	if(exp[0] != 0) {
		printf("%s\n", exp);
	}
	SIM_debugBreak();
}

void SIM_handleAssertionFailureMsg(const char* filename, unsigned int lineno, const char* fmt, ...) {
	printf("[SIM ASSERTION FAILED] %s:%d\n", filename, lineno);
	va_list vlist;
	va_start(vlist, fmt);
	vprintf(fmt, vlist);
	va_end(vlist);
	printf("\n");
	SIM_debugBreak();
}

void SIM_debugBreak() {
	#ifdef SDK_BUILD_WIN64
	if(IsDebuggerPresent()){
		__debugbreak();
	}
	#endif
    #if defined(SDK_BUILD_LINUX) || defined(SDK_BUILD_MACOS)
    raise(SIGTRAP);
    #endif
}