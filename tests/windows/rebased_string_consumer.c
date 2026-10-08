#include <windows.h>
#include <stdint.h>
#include <stdio.h>

/* Caustic exports retain its internal System V convention; foreign calls
 * bridge to Win64. The zero-argument fixture still requires the caller to
 * treat rdi/rsi as clobbered rather than Win64-preserved registers. */
typedef int (__attribute__((sysv_abi)) *consumer_fn)(void);

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    FILE *file = fopen(argv[1], "rb");
    if (!file) return 3;
    IMAGE_DOS_HEADER dos;
    IMAGE_NT_HEADERS64 pe;
    if (fread(&dos, sizeof dos, 1, file) != 1 || dos.e_magic != IMAGE_DOS_SIGNATURE
        || fseek(file, dos.e_lfanew, SEEK_SET) != 0
        || fread(&pe, sizeof pe, 1, file) != 1
        || pe.Signature != IMAGE_NT_SIGNATURE
        || pe.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        fclose(file);
        return 4;
    }
    fclose(file);
    uintptr_t preferred = (uintptr_t)pe.OptionalHeader.ImageBase;
    void *reservation = VirtualAlloc((void *)preferred, pe.OptionalHeader.SizeOfImage,
                                     MEM_RESERVE, PAGE_NOACCESS);
    if (!reservation) {
        MEMORY_BASIC_INFORMATION info;
        if (!VirtualQuery((void *)preferred, &info, sizeof info) || info.State == MEM_FREE)
            return 5;
    }
    HMODULE dll = LoadLibraryA(argv[1]);
    int result = 6;
    if (dll) {
        result = 7;
        if ((uintptr_t)dll != preferred) {
            union { FARPROC address; consumer_fn callable; } entry;
            entry.address = GetProcAddress(dll, "main");
            result = entry.address ? entry.callable() : 8;
            if (result == 0)
                printf("rebased mutable/custom strings: %llx -> %llx passed\n",
                       (unsigned long long)preferred, (unsigned long long)(uintptr_t)dll);
        }
        FreeLibrary(dll);
    }
    if (reservation) VirtualFree(reservation, 0, MEM_RELEASE);
    return result;
}
