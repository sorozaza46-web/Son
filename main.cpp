#include <windows.h>
#include <fwpmu.h>
#include <iostream>
#include <tlhelp32.h>

#pragma comment(lib, "Fwpuclnt.lib")

HANDLE gEngineHandle = NULL;
UINT64 gFilterIdTCP = 0;
UINT64 gFilterIdUDP = 0;
bool gIsLagging = false;

// Çalışan oyunun PID'sini (Process ID) otomatik bulur
DWORD GetProcessIdByName(const wchar_t* processName) {
    DWORD pid = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W processEntry;
        processEntry.dwSize = sizeof(processEntry);
        if (Process32FirstW(snapshot, &processEntry)) {
            do {
                if (wcscmp(processEntry.szExeFile, processName) == 0) {
                    pid = processEntry.th32ProcessID;
                    break;
                }
            } while (Process32NextW(snapshot, &processEntry));
        }
        CloseHandle(snapshot);
    }
    return pid;
}

// PID üzerinden uygulamanın WFP AppID (Blob) verisini dinamik olarak alır
FWP_BYTE_BLOB* GetAppIdFromPid(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProcess) return NULL;

    wchar_t buffer[MAX_PATH];
    DWORD size = MAX_PATH;
    FWP_BYTE_BLOB* appId = NULL;

    if (QueryFullProcessImageNameW(hProcess, 0, buffer, &size)) {
        FwpmGetAppIdFromFileName0(buffer, &appId);
    }
    CloseHandle(hProcess);
    return appId;
}

void StartLag(FWP_BYTE_BLOB* appId) {
    if (gIsLagging || !appId) return;

    FWPM_FILTER0 filter = { 0 };
    FWPM_FILTER_CONDITION0 condition = { 0 };

    filter.displayData.name = L"SonOyuncuUniversalFilter";
    filter.action.type = FWP_ACTION_BLOCK; // Paketleri Engelle
    filter.weight.type = FWP_EMPTY;
    filter.numFilterConditions = 1;

    // Koşul: Sadece bu uygulamanın paketleri
    condition.fieldKey = FWPM_CONDITION_ALE_APP_ID;
    condition.matchType = FWP_MATCH_EQUAL;
    condition.conditionValue.type = FWP_BYTE_BLOB_TYPE;
    condition.conditionValue.byteBlob = appId;
    filter.filterCondition = &condition;

    // 1. KATMAN: Hem Giden TCP Bağlantılarını Kapat
    filter.layerKey = FWPM_LAYER_ALE_AUTH_CONNECT_V4;
    FwpmFilterAdd0(gEngineHandle, &filter, NULL, &gFilterIdTCP);

    // 2. KATMAN: Hem de Giden UDP (Datagram) Paketlerini Kapat
    filter.layerKey = FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V4;
    FwpmFilterAdd0(gEngineHandle, &filter, NULL, &gFilterIdUDP);

    gIsLagging = true;
    std::cout << "[+] LAG AKTIF! (Oyunun tum ag trafigi kesildi)\n";
}

void StopLag() {
    if (!gIsLagging) return;

    FwpmFilterDeleteById0(gEngineHandle, gFilterIdTCP);
    FwpmFilterDeleteById0(gEngineHandle, gFilterIdUDP);
    gIsLagging = false;
    std::cout << "[+] LAG KAPATILDI. Ag akisi normale dondu.\n";
}

int main() {
    // WFP Engine Açılışı
    DWORD result = FwpmEngineOpen0(NULL, RPC_C_AUTHN_WINNT, NULL, NULL, &gEngineHandle);
    if (result != ERROR_SUCCESS) {
        std::cout << "[-] Lutfen programi YONETICI OLARAK calistirin.\n";
        std::cin.get();
        return 1;
    }

    std::cout << "=== SonOyuncu Dinamik Blink Lag v2.0 ===\n";
    std::cout << "[*] Oyun araniyor, lutfen SonOyuncu'yu acik tutun...\n";

    DWORD pid = 0;
    while (pid == 0) {
        pid = GetProcessIdByName(L"sonoyuncuclient.exe");
        if (pid == 0) {
            Sleep(1000); // Oyun açılana kadar saniyede bir tara
        }
    }

    std::cout << "[+] Oyun bulundu! PID: " << pid << "\n";
    FWP_BYTE_BLOB* appId = GetAppIdFromPid(pid);
    
    if (!appId) {
        std::cout << "[-] Uygulama kimligi alinamadi.\n";
        FwpmEngineClose0(gEngineHandle);
        return 1;
    }

    std::cout << "[*] Sistem Hazir! X tusuna basili tutarak lag yapabilirsiniz.\n\n";

    while (true) {
        if (GetAsyncKeyState('X') & 0x8000) {
            StartLag(appId);
        } else {
            StopLag();
        }
        Sleep(30);
    }

    FwpmFreeMemory0((void**)&appId);
    FwpmEngineClose0(gEngineHandle);
    return 0;
}
