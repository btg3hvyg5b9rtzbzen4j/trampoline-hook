#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <stdio.h>
#include <d3d9.h>

// Offset to IDirect3DDevice9::EndScene from d3d9.dll base
// May differ for you, please use a debugger to find it yourself
#define O_ENDSCENE 0x2BC20

// Global variable to hold the trampoline function address
uintptr_t EndSceneTrampoline;

void EndSceneHook(IDirect3DDevice9* pDevice) {
    // Use the device (which is passed as first argument since its a member function) to draw a red rectangle
    D3DRECT rect = { 50, 50, 200, 200 };
    pDevice->lpVtbl->Clear(pDevice, 1, &rect, D3DCLEAR_TARGET, D3DCOLOR_XRGB(255, 0, 0), 1.0f, 0);

    // Cast and call the trampoline to execute original instructions and original function flow
    // We have to pass pDevice otherwise the stack will be corrupted and cause a crash
    // Note: (void(*)()) is the function pointer type cast (as seen with typedef, just to clear it up)
    // Note 2: I did it like this to keep it simple and understandable, but you can also typedef the function pointer type
    ( (void(*)()) EndSceneTrampoline )(pDevice);
}

uintptr_t TrampolineHook(uintptr_t aFuncToHook, uintptr_t aHookFunc, int nStolenBytes) {
    // Ensure we have at least 12 bytes which is the size of a 64-bit absolute jump instruction
    if (nStolenBytes < 12) {
        printf("Not enough bytes stolen for hook\n");
        return 0;
    }

    // Create the trampoline function which will execute the stolen bytes and jump back to the original function
    // We allocate nStolenBytes + 12 for the jump instruction
    uintptr_t aTrampoline = (uintptr_t)VirtualAlloc(0, nStolenBytes + 12, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    
    if (!aTrampoline) {
        printf("VirtualAlloc failed\n");
        return 0;
    } else {
        printf("Trampoline allocated at: 0x%llX\n", aTrampoline);
    }

    // Copy the bytes that will be overwritten by the jump to our hook function
    memcpy((char*)aTrampoline, (char*)aFuncToHook, nStolenBytes);

    // Create the absolute jump instruction and populate it with the address to jump back to
    // The address to jump back to will be original + nStolenBytes (so we continue normal execution after our hook)
    char jmpBack[12] = {
        0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // mov rax, <address>
        0xFF, 0xE0                                                  // jmp rax
    };
    *(uintptr_t*)(jmpBack + 2) = aFuncToHook + nStolenBytes;
    memcpy((char*)(aTrampoline + nStolenBytes), jmpBack, sizeof(jmpBack));
    
    printf("Jump back instruction written at: 0x%llX\n", aTrampoline + nStolenBytes);

    // Patch the original function with the jump to our hook
    DWORD oldProtect;
    VirtualProtect((LPVOID)aFuncToHook, nStolenBytes, PAGE_EXECUTE_READWRITE, &oldProtect);

    // Again create a jump instruction, this time to our hook function
    char jmpHook[12] = {
        0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // mov rax, <address>
        0xFF, 0xE0                                                  // jmp rax
    };
    *(uintptr_t*)(jmpHook + 2) = aHookFunc;
    memcpy((char*)aFuncToHook, jmpHook, sizeof(jmpHook));

    printf("Hook instruction written at: 0x%llX\n", aFuncToHook);

    // Fill the remaining bytes with NOPs if any
    // This ensures we don't leave any partial instructions and corrupt the original function
    for (int i = 12; i < nStolenBytes; i++) {
        *((char*)aFuncToHook + i) = 0x90;
    }

    VirtualProtect((LPVOID)aFuncToHook, nStolenBytes, oldProtect, &oldProtect);

    // Return the trampoline function address so it can be called from the hook
    return aTrampoline;
}

void TrampolineUnhook(uintptr_t aHookedFunc, uintptr_t aTrampoline, int nStolenBytes) {
    // Literally just restore the old bytes from the trampoline back to the original function
    DWORD oldProtect;
    VirtualProtect((LPVOID)aHookedFunc, nStolenBytes, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy((char*)aHookedFunc, (char*)aTrampoline, nStolenBytes);
    VirtualProtect((LPVOID)aHookedFunc, nStolenBytes, oldProtect, &oldProtect);
    VirtualFree((LPVOID)aTrampoline, nStolenBytes + 12, MEM_RELEASE);
}

DWORD WINAPI MainThread(LPVOID lpThreadParameter)
{
    HINSTANCE hinstDLL = (HINSTANCE)lpThreadParameter;

    AllocConsole();
    FILE *fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);

    printf("Injected\n");

    uintptr_t endSceneAddy = (uintptr_t)GetModuleHandle("d3d9.dll") + O_ENDSCENE;

    EndSceneTrampoline = TrampolineHook(endSceneAddy, (uintptr_t)EndSceneHook, 13);

    while (!GetAsyncKeyState(VK_END)) {
        Sleep(67);
    }

    TrampolineUnhook(endSceneAddy, EndSceneTrampoline, 13);

    FreeConsole();
    fclose(fp);
    FreeLibraryAndExitThread(hinstDLL, 0);
    return TRUE;
}

BOOL WINAPI DllMain(
    HINSTANCE hinstDLL,
    DWORD fdwReason)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        HANDLE thread = CreateThread(0, 0, MainThread, hinstDLL, 0, 0);
        if (thread)
        {
            CloseHandle(thread);
        }
    }
    return TRUE;
}