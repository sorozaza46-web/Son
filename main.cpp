#include <windows.h>
#include <fwpmu.h>
#include <iostream>

#pragma comment(lib, "Fwpuclnt.lib")

HANDLE gEngineHandle = NULL;
UINT64 gFilterId = 0; // HATA ÇÖZÜMÜ: UINT32 yerine UINT64 yapildi
bool gIsLagging = false;

// WFP Filtresini Başlat (Sadece hedef oyunu engeller)
void StartLag(UINT16 port, FWP_BYTE_BLOB* appId) {
    if (gIsLagging) return;

    FWPM_FILTER0 filter = { 0 };
    FWPM_FILTER_CONDITION0 conditions[2] = { 0 }; 

    filter.displayData.name = L"SonOyuncuIsolatedBlinkFilter";
    filter.layerKey = FWPM_LAYER_ALE_AUTH_CONNECT_V4; 
    filter.action.type = FWP_ACTION_BLOCK;             
    filter.weight.type = FWP_EMPTY;
    filter.numFilterConditions = 2; 

    // 1. KOŞUL: Uzak Port -> 443
    conditions[0].fieldKey = FWPM_CONDITION_IP_REMOTE_PORT;
    conditions[0].matchType = FWP_MATCH_EQUAL;
    conditions[0].conditionValue.type = FWP_UINT16;
    conditions[0].conditionValue.uint16 = port;

    // 2. KOŞUL: Uygulama -> sonoyuncuclient.exe
    conditions[1].fieldKey = FWPM_CONDITION_ALE_APP_ID;
    conditions[1].matchType = FWP_MATCH_EQUAL;
    conditions[1].conditionValue.type = FWP_BYTE_BLOB_TYPE;
    conditions[1].conditionValue.byteBlob = appId;

    filter.filterCondition = conditions;

    // Artik gFilterId UINT64* tipinde oldugu icin C2664 hatasi cozuldu
    DWORD result = FwpmFilterAdd0(gEngineHandle, &filter, NULL, &gFilterId);
    if (result == ERROR_SUCCESS) {
        gIsLagging = true;
        std::cout << "[+] LAG AKTIF! (Sadece SonOyuncu TCP 443 engelleniyor...)\n";
    } else {
        std::cout << "[-] Filtre ekleme hatasi: " << result << "\n";
    }
}

// WFP Filtresini Kaldır
void StopLag() {
    if (!gIsLagging) return;

    DWORD result = FwpmFilterDeleteById0(gEngineHandle, gFilterId);
    if (result == ERROR_SUCCESS) {
        gIsLagging = false;
        std::cout << "[+] LAG KAPATILDI. Paket akisi normale dondu.\n";
    }
}

int main() {
    UINT16 gamePort = 443; 
    PCWSTR exePath = L"C:\\Users\\MONSTER\\AppData\\Roaming\\.sonoyuncu\\sonoyuncuclient.exe"; 

    DWORD result = FwpmEngineOpen0(NULL, RPC_C_AUTHN_WINNT, NULL, NULL, &gEngineHandle);
    if (result != ERROR_SUCCESS) {
        std::cout << "[-] Basarisiz! Lutfen programi YONETICI OLARAK (Admin) calistirin.\n";
        return 1;
    }

    FWP_BYTE_BLOB* appId = NULL;
    result = FwpmGetAppIdFromFileName0(exePath, &appId);
    if (result != ERROR_SUCCESS) {
        std::cout << "[-] Dosya yolu bulunamadi! exePath alanini kontrol edin.\n";
        FwpmEngineClose0(gEngineHandle);
        return 1;
    }

    std::cout << "=======================================\n";
    std::cout << "   SonOyuncu Nokta Atisi Blink Lag    \n";
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
