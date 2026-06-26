#include <windows.h>
#include <iostream>
#include <vector>
#include <queue>

// Orijinal Winsock send fonksiyonunun imza yapısı
typedef int(WSAAPI* tSend)(SOCKET s, const char* buf, int len, int flags);
tSend oSend = NULL;
void* trampolineMemory = NULL; // Trambolin için ayrılan bellek alanı

// Paketleri geçici olarak tutacağımız kuyruk yapısı
struct NetworkPacket {
    SOCKET socket;
    std::vector<char> buffer;
    int flags;
};
std::queue<NetworkPacket> packetQueue;

bool gIsBlinking = false;
CRITICAL_SECTION queueCriticalSection;

// Kancalanmış (Hooked) Send Fonksiyonu
int WSAAPI hkSend(SOCKET s, const char* buf, int len, int flags) {
    // X tuşuna basılı tutuluyorsa paketleri gönderme, hafızaya kaydet
    if (GetAsyncKeyState('X') & 0x8000) {
        if (!gIsBlinking) {
            gIsBlinking = true;
        }

        EnterCriticalSection(&queueCriticalSection);
        NetworkPacket pkt;
        pkt.socket = s;
        pkt.buffer.assign(buf, buf + len);
        pkt.flags = flags;
        packetQueue.push(pkt);
        LeaveCriticalSection(&queueCriticalSection);

        // Oyuna paketin başarıyla gittiğini söylüyoruz (Bypass noktası)
        return len; 
    }

    // X tuşu bırakıldıysa ve içeride birikmiş paket varsa hepsini sırayla gönder
    if (gIsBlinking) {
        EnterCriticalSection(&queueCriticalSection);
        while (!packetQueue.empty()) {
            NetworkPacket pkt = packetQueue.front();
            // oSend artık trambolini işaret ettiği için güvenle doğrudan çağrılabilir
            oSend(pkt.socket, pkt.buffer.data(), static_cast<int>(pkt.buffer.size()), pkt.flags);
            packetQueue.pop();
        }
        gIsBlinking = false;
        LeaveCriticalSection(&queueCriticalSection);
    }

    // Normal durumlarda trambolin üzerinden orijinal send fonksiyonunu çalıştır
    return oSend(s, buf, len, flags);
}

// Güvenli Trambolin (Trampoline) destekli Satır İçi Kancalama Mekanizması
void HookSend() {
    HMODULE hWs2 = GetModuleHandleA("ws2_32.dll");
    if (!hWs2) return;

    void* pSend = (void*)GetProcAddress(hWs2, "send");
    if (!pSend) return;

    // 64-bit JUMP (Hook) kodu (Tam olarak 12 Bayt)
    unsigned char jmpCode[] = {
        0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // mov rax, absolute_address
        0xFF, 0xE0                                                 // jmp rax
    };

    size_t hookSize = sizeof(jmpCode); // 12 Bayt

    // --- TRAMBOLİN OLUŞTURMA (Sonsuz döngüyü engellemek için) ---
    // Orijinal fonksiyondan çalacağımız 12 baytı ve geri dönüş jump'ını tutacak dinamik bellek ayırıyoruz
    trampolineMemory = VirtualAlloc(NULL, hookSize + hookSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!trampolineMemory) return;

    // 1. Çalınan ilk 12 baytı trambolin belleğinin başına kopyala
    memcpy(trampolineMemory, pSend, hookSize);

    // 2. Trambolinin sonuna, orijinal send fonksiyonunun kaldığı yere (pSend + 12) geri dönecek JUMP kodunu yaz
    unsigned char trmpJmpCode[12];
    memcpy(trmpJmpCode, jmpCode, hookSize);
    uint64_t returnAddr = (uint64_t)pSend + hookSize;
    memcpy(&trmpJmpCode[2], &returnAddr, sizeof(returnAddr));
    memcpy((char*)trampolineMemory + hookSize, trmpJmpCode, hookSize);

    // oSend fonksiyonunu bu tramboline yönlendiriyoruz. Artık çağrıldığında orijinal akış bozulmaz.
    oSend = (tSend)trampolineMemory;
    // -------------------------------------------------------------

    // Orijinal send fonksiyonunun başına kendi hkSend adresimizi yerleştiriyoruz
    DWORD oldProtect;
    VirtualProtect(pSend, hookSize, PAGE_EXECUTE_READWRITE, &oldProtect);

    uint64_t hookAddr = (uint64_t)hkSend;
    memcpy(&jmpCode[2], &hookAddr, sizeof(hookAddr));
    memcpy(pSend, jmpCode, hookSize);

    VirtualProtect(pSend, hookSize, oldProtect, &oldProtect);
}

DWORD WINAPI MainThread(LPVOID lpParam) {
    InitializeCriticalSection(&queueCriticalSection);
    HookSend();
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        CreateThread(NULL, 0, MainThread, NULL, 0, NULL);
        break;
    case DLL_PROCESS_DETACH:
        // Ayrılan trambolin belleğini serbest bırak
        if (trampolineMemory) {
            VirtualFree(trampolineMemory, 0, MEM_RELEASE);
        }
        DeleteCriticalSection(&queueCriticalSection);
        break;
    }
    return TRUE;
}
