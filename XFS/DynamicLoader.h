#ifndef PCSC_CENXFS_BRIDGE_XFS_DYNAMICLOADER_H
#define PCSC_CENXFS_BRIDGE_XFS_DYNAMICLOADER_H

#pragma once

#include <windows.h>
#include <string>
#include <stdexcept>
#include <XFSAPI.H>  // Pour les définitions XFS
#include <XFSCONF.H>  // Pour les fonctions de configuration XFS

#ifdef WFS_DYNAMIC_LOAD
#define WFS_IMPORT __declspec(dllimport)
#else
#define WFS_IMPORT
#endif

namespace XFS {

class DynamicLoader {
public:
    // Types pour les fonctions XFS de base
    typedef HRESULT (WINAPI *WFSCleanUp_t)(void);
    typedef HRESULT (WINAPI *WFSAsyncClose_t)(HSERVICE hService, HWND hWnd, LPREQUESTID lpRequestID);
    typedef HRESULT (WINAPI *WFSAsyncExecute_t)(HSERVICE hService, DWORD dwCommand, LPVOID lpCmdData, DWORD dwTimeOut, HWND hWnd, LPREQUESTID lpRequestID);
    typedef HRESULT (WINAPI *WFSAsyncGetInfo_t)(HSERVICE hService, DWORD dwCategory, LPVOID lpQueryDetails, DWORD dwTimeOut, HWND hWnd, LPREQUESTID lpRequestID);
    typedef HRESULT (WINAPI *WFSAsyncOpen_t)(LPSTR lpszLogicalName, HAPP hApp, LPSTR lpszAppID, DWORD dwTraceLevel, DWORD dwTimeOut, LPHSERVICE lphService, HWND hWnd, DWORD dwSrvcVersionsRequired, LPWFSVERSION lpSrvcVersion, LPWFSVERSION lpSPIVersion, LPREQUESTID lpRequestID);
    typedef HRESULT (WINAPI *WFSStartUp_t)(DWORD dwVersionsRequired, LPWFSVERSION lpWFSVersion);

    // Types pour les fonctions de configuration XFS
    typedef HRESULT (WINAPI *WFMCloseKey_t)(HKEY hKey);
    typedef HRESULT (WINAPI *WFMOpenKey_t)(HKEY hKey, LPSTR lpszSubKey, PHKEY phkResult);
    typedef HRESULT (WINAPI *WFMQueryValue_t)(HKEY hKey, LPSTR lpszValueName, LPSTR lpszData, LPDWORD lpcchData);

private:
    HMODULE hMsxfs = NULL;
    HMODULE hXfsConf = NULL;

    // Variables pour stocker les pointeurs de fonction XFS de base
    WFSCleanUp_t WFSCleanUp = nullptr;
    WFSAsyncClose_t WFSAsyncClose = nullptr;
    WFSAsyncExecute_t WFSAsyncExecute = nullptr;
    WFSAsyncGetInfo_t WFSAsyncGetInfo = nullptr;
    WFSAsyncOpen_t WFSAsyncOpen = nullptr;
    WFSStartUp_t WFSStartUp = nullptr;

    // Variables pour stocker les pointeurs de fonction de configuration
    WFMCloseKey_t WFMCloseKey = nullptr;
    WFMOpenKey_t WFMOpenKey = nullptr;
    WFMQueryValue_t WFMQueryValue = nullptr;

public:
    DynamicLoader() = default;
    ~DynamicLoader() {
        unload();
    }

    void load() {
        // Charger les DLLs
        hMsxfs = LoadLibraryA("msxfs.dll");
        if (!hMsxfs) throw std::runtime_error("Failed to load msxfs.dll");

        hXfsConf = LoadLibraryA("xfs_conf.dll");
        if (!hXfsConf) throw std::runtime_error("Failed to load xfs_conf.dll");

        // Charger les fonctions de msxfs.dll
        WFSCleanUp = (WFSCleanUp_t)GetProcAddress(hMsxfs, "WFSCleanUp");
        WFSAsyncClose = (WFSAsyncClose_t)GetProcAddress(hMsxfs, "WFSAsyncClose");
        WFSAsyncExecute = (WFSAsyncExecute_t)GetProcAddress(hMsxfs, "WFSAsyncExecute");
        WFSAsyncGetInfo = (WFSAsyncGetInfo_t)GetProcAddress(hMsxfs, "WFSAsyncGetInfo");
        WFSAsyncOpen = (WFSAsyncOpen_t)GetProcAddress(hMsxfs, "WFSAsyncOpen");
        WFSStartUp = (WFSStartUp_t)GetProcAddress(hMsxfs, "WFSStartUp");

        // Charger les fonctions de configuration de xfs_conf.dll
        WFMCloseKey = (WFMCloseKey_t)GetProcAddress(hXfsConf, "WFMCloseKey");
        WFMOpenKey = (WFMOpenKey_t)GetProcAddress(hXfsConf, "WFMOpenKey");
        WFMQueryValue = (WFMQueryValue_t)GetProcAddress(hXfsConf, "WFMQueryValue");

        // Vérifier que toutes les fonctions ont été chargées
        if (!WFSCleanUp || !WFSAsyncClose || !WFSAsyncExecute || !WFSAsyncGetInfo ||
            !WFSAsyncOpen || !WFSStartUp || !WFMCloseKey || !WFMOpenKey || !WFMQueryValue) {
            unload();
            throw std::runtime_error("Failed to load required XFS functions");
        }
    }

    void unload() {
        if (hMsxfs) {
            FreeLibrary(hMsxfs);
            hMsxfs = NULL;
        }
        if (hXfsConf) {
            FreeLibrary(hXfsConf);
            hXfsConf = NULL;
        }

        // Réinitialiser les pointeurs de fonction XFS de base
        WFSCleanUp = nullptr;
        WFSAsyncClose = nullptr;
        WFSAsyncExecute = nullptr;
        WFSAsyncGetInfo = nullptr;
        WFSAsyncOpen = nullptr;
        WFSStartUp = nullptr;

        // Réinitialiser les pointeurs de fonction de configuration
        WFMCloseKey = nullptr;
        WFMOpenKey = nullptr;
        WFMQueryValue = nullptr;
    }

    // Getters pour les fonctions XFS de base
    WFSCleanUp_t getWFSCleanUp() const { return WFSCleanUp; }
    WFSAsyncClose_t getWFSAsyncClose() const { return WFSAsyncClose; }
    WFSAsyncExecute_t getWFSAsyncExecute() const { return WFSAsyncExecute; }
    WFSAsyncGetInfo_t getWFSAsyncGetInfo() const { return WFSAsyncGetInfo; }
    WFSAsyncOpen_t getWFSAsyncOpen() const { return WFSAsyncOpen; }
    WFSStartUp_t getWFSStartUp() const { return WFSStartUp; }

    // Getters pour les fonctions de configuration
    WFMCloseKey_t getWFMCloseKey() const { return WFMCloseKey; }
    WFMOpenKey_t getWFMOpenKey() const { return WFMOpenKey; }
    WFMQueryValue_t getWFMQueryValue() const { return WFMQueryValue; }

    bool isLoaded() const {
        return hMsxfs != NULL && hXfsConf != NULL;
    }
};

} // namespace XFS

#endif // PCSC_CENXFS_BRIDGE_XFS_DYNAMICLOADER_H 