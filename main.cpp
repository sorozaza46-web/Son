#include <windows.h>
#include <fwpmu.h>
#include <iostream>
#include <tlhelp32.h>

#pragma comment(lib, "Fwpuclnt.lib")

HANDLE gEngineHandle = NULL;
UINT32 gFilterId = 0;
bool gIsLagging = false;

void StartLag(UINT16 port, FWP_BYTE_BLOB* appId) {
    if (gIsLagging) return;

    FWPM_FILTER0 filter = { 0 };
    FWPM_FILTER_CONDITION0 conditions[2] = { 0 };

    filter.displayData.name = L"SonOyuncuIsolatedBlinkFilter";
    filter.layerKey = FWPM_LAYER_ALE_AUTH_CONNECT_V4; 
    filter.action.type = FWP_ACTION_BLOCK;             
    filter.weight.type = FWP_EMPTY;
    filter.numFilterConditions = 2; 

    conditions[0].fieldKey = FWPM_CONDITION_IP_REMOTE_PORT;
    conditions[0].matchType = FWP_MATCH_EQUAL;
    conditions[0].conditionValue.type = FWP_UINT16;
    conditions[0].conditionValue.uint16 = port;

    conditions[1].fieldKey = FWPM_CONDITION_ALE_APP_ID;
    conditions[1].matchType = FWP_MATCH_EQUAL;
    conditions[1].conditionValue.type = FWP_BYTE_BLOB_TYPE;
    conditions[1].conditionValue.byteBlob = appId;

    filter.filterCondition = conditions;

    DWORD result = FwpmFilterAdd0(gEngineHandle, &filter, NULL, &gFilterId);
    if (result == ERROR_SUCCESS) {
        gIsLagging = true;
        std::cout << "[+] LAG AKTIF! (X basili tutuluyor)\n";
    }
}

void StopLag() {
    if (!gIsLagging) return;

    DWORD result = FwpmFilterDeleteById0(gEngineHandle, gFilterId);
    if (result == ERROR_SUCCESS) {
        gIsLagging = false;
        std::cout << "[+] LAG KAPATILDI. Paketler birakildi.\n";
    }
}

int main() {
    UINT16 gamePort = 443; 
    PCWSTR exePath = L"C:\\Users\\MONSTER\\AppData\\Roaming\\.sonoyuncu\\sonoyuncuclient.exe"; 

    DWORD result = FwpmEngineOpen0(NULL, RPC_C_AUTHN_WINNT, NULL, NULL, &gEngineHandle);
    if (result != ERROR_SUCCESS) {
        std::cout << "[-] Lutfen programi YONETICI OLARAK calistirin.\n";
        std::cin.get();
        return 1;
    }

    FWP_BYTE_BLOB* appId = NULL;
    result = FwpmGetAppIdFromFileName0(exePath, &appId);
    if (result != ERROR_SUCCESS) {
        std::cout << "[-] Statik dosya yolu bulunamadi. Oyunun kurulu oldugundan emin olun.\n";
        FwpmEngineClose0(gEngineHandle);
        std::cin.get();
        return 1;
    }

    std::cout << "=======================================\n";
    std::cout << "   SonOyuncu Isolated Blink Lag v1.0   \n";
    std::cout << "=======================================\n";
    std::cout << "[*] Hedef Port: " << gamePort << " (TCP)\n";
    std::cout << "[*] Kullanim: X tusuna BASILI TUTUNCA lag girer.\n\n";

    while (true) {
        if (GetAsyncKeyState('X') & 0x8000) {
            StartLag(gamePort, appId);
        } else {
            StopLag();
        }
        Sleep(30); 
    }

    FwpmFreeMemory0((void**)&appId);
    FwpmEngineClose0(gEngineHandle);
    return 0;
}

