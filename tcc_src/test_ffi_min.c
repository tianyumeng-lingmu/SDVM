/* Test TCC GetProcAddress - using same declarations as SDVM */
#include <stdio.h>
#include <stdint.h>

typedef void* HMODULE;
HMODULE LoadLibraryA(const char*);
void* GetProcAddress(HMODULE, const char*);
int FreeLibrary(HMODULE);

typedef int64_t (*ffi_fn_0)();

int main() {
    HMODULE hMod = LoadLibraryA("kernel32.dll");
    printf("LoadLibraryA: %p\n", (void*)hMod);

    if (hMod) {
        void* p1 = (void*)GetProcAddress(hMod, "GetLastError");
        void* p2 = (void*)GetProcAddress(hMod, "GetCurrentProcessId");
        void* p3 = (void*)GetProcAddress(hMod, "IsDebuggerPresent");

        printf("GetLastError: %p\n", p1);
        printf("GetCurrentProcessId: %p\n", p2);
        printf("IsDebuggerPresent: %p\n", p3);

        if (p2) {
            ffi_fn_0 fn = (ffi_fn_0)p2;
            int64_t pid = fn();
            printf("PID: %lld\n", (long long)pid);
        }

        FreeLibrary(hMod);
    }
    return 0;
}
