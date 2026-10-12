/* MSVC SEH for os_set_thread_name. RaiseException is caught here so it
 * does not abort the process; a debugger still sees 0x406D1388.
 * threading-windows.c does the same __try/__except. MinGW skips it. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#pragma pack(push, 8)
struct vs_threadname_info {
	DWORD type; /* 0x1000 */
	const char *name;
	DWORD thread_id;
	DWORD flags;
};
#pragma pack(pop)

void obs_rust_raise_thread_name_exception(const char *name)
{
	struct vs_threadname_info info;
	info.type = 0x1000;
	info.name = name;
	info.thread_id = GetCurrentThreadId();
	info.flags = 0;

	__try {
		RaiseException(0x406D1388, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR *)&info);
	} __except (EXCEPTION_EXECUTE_HANDLER) {
	}
}
