#include <windows.h>
#include <iostream>
#include <vector>
#include <queue>

// Orijinal Winsock send fonksiyonunun imza yapısı
typedef int(WSAAPI* tSend)(SOCKET s, const char* buf, int len, int flags);
tSend oSend = NULL;

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
            oSend(pkt.socket, pkt.buffer.data(), pkt.buffer.size(), pkt.flags);
            packetQueue.pop();
        }
        gIsBlinking = false;
        LeaveCriticalSection(&queueCriticalSection);
    }

    // Normal durumlarda orijinal send fonksiyonunu çalıştır
    return oSend(s, buf, len, flags);
}

// Basit satır içi kancalama (Inline Hook) mekanizması
void HookSend() {
    HMODULE hWs2 = GetModuleHandleA("ws2_32.dll");
    if (!hWs2) return;

    void* pSend = (void*)GetProcAddress(hWs2, "send");
    if (!pSend) return;

    // Orijinal fonksiyonun adresini sakla
    oSend = (tSend)pSend;

    // 64-bit JUMP (Hook) kodunu yerleştirme işlemi
    DWORD oldProtect;
    VirtualProtect(pSend, 13, PAGE_EXECUTE_READWRITE, &oldProtect);

    // mov rax, hkSend; jmp rax
    unsigned char jmpCode[] = {
        0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // mov rax, absolute_address
        0xFF, 0xE0                                                 // jmp rax
    };
    
    uint64_t hookAddr = (uint64_t)hkSend;
    memcpy(&jmpCode[2], &hookAddr, sizeof(hookAddr));
    memcpy(pSend, jmpCode, sizeof(jmpCode));

    VirtualProtect(pSend, 13, oldProtect, &oldProtect);
}

DWORD WINAPI MainThread(LPVOID lpParam) {
    InitializeCriticalSection(&queueCriticalSection);
    HookSend();
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        CreateThread(NULL, 0, MainThread, NULL, 0, NULL);
        break;
    case DLL_PROCESS_DETACH:
        DeleteCriticalSection(&queueCriticalSection);
        break;
    }
    return TRUE;
}
