#ifndef PCSC_CENXFS_BRIDGE_XFS_XFSMANAGER_H
#define PCSC_CENXFS_BRIDGE_XFS_XFSMANAGER_H

#pragma once

#include "DynamicLoader.h"
#include <memory>

namespace XFS {

class XFSManager {
private:
    static std::unique_ptr<XFSManager> instance;
    DynamicLoader loader;
    bool initialized = false;

    XFSManager() = default;

    // Déclarer std::unique_ptr comme ami pour qu'il puisse accéder au destructeur privé
    friend class std::unique_ptr<XFSManager>;

public:
    ~XFSManager() {
        if (initialized) {
            loader.unload();
        }
    }

    static XFSManager& getInstance() {
        if (!instance) {
            instance = std::unique_ptr<XFSManager>(new XFSManager());
        }
        return *instance;
    }

    void initialize() {
        if (!initialized) {
            loader.load();
            initialized = true;
        }
    }

    bool isInitialized() const {
        return initialized;
    }

    // Getters pour les fonctions XFS
    DynamicLoader::WFSStartUp_t getWFSStartUp() const { return loader.getWFSStartUp(); }
    DynamicLoader::WFSCleanUp_t getWFSCleanUp() const { return loader.getWFSCleanUp(); }
    DynamicLoader::WFSAsyncOpen_t getWFSAsyncOpen() const { return loader.getWFSAsyncOpen(); }
    DynamicLoader::WFSAsyncClose_t getWFSAsyncClose() const { return loader.getWFSAsyncClose(); }
    DynamicLoader::WFSAsyncExecute_t getWFSAsyncExecute() const { return loader.getWFSAsyncExecute(); }
    DynamicLoader::WFSAsyncGetInfo_t getWFSAsyncGetInfo() const { return loader.getWFSAsyncGetInfo(); }

    // Getters pour les fonctions de configuration XFS
    DynamicLoader::WFMCloseKey_t getWFMCloseKey() const { return loader.getWFMCloseKey(); }
    DynamicLoader::WFMOpenKey_t getWFMOpenKey() const { return loader.getWFMOpenKey(); }
    DynamicLoader::WFMQueryValue_t getWFMQueryValue() const { return loader.getWFMQueryValue(); }
};

} // namespace XFS

#endif // PCSC_CENXFS_BRIDGE_XFS_XFSMANAGER_H 