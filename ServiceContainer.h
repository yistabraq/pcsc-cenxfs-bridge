#ifndef PCSC_CENXFS_BRIDGE_ServiceContainer_H
#define PCSC_CENXFS_BRIDGE_ServiceContainer_H

#pragma once

#include <map>

// PC/CS API -- pour SCARD_READERSTATE
#include <winscard.h>
// Définitions pour les lecteurs de cartes (Identification card unit (IDC)) -- pour HSERVICE
#include <XFSIDC.h>

class Manager;
class Service;
class Settings;
class ServiceContainer {
    /// Type pour mapper les services XFS aux cartes PC/SC.
    typedef std::map<HSERVICE, Service*> ServiceMap;
private:
    /// Liste des cartes ouvertes pour l'interaction avec le système XFS.
    ServiceMap services;
public:
    ~ServiceContainer();
public:
    /** Vérifie que le handle de service spécifié est un handle de carte valide. */
    inline bool isValid(HSERVICE hService) const {
        return services.find(hService) != services.end();
    }
    /** @return true si aucun service n'est enregistré dans le conteneur. */
    inline bool isEmpty() const { return services.empty(); }

    Service& create(Manager& manager, HSERVICE hService, const Settings& settings);
    Service& get(HSERVICE hService);
    void remove(HSERVICE hService);
public:// Abonnement aux événements et génération d'événements
    /** Ajoute la fenêtre spécifiée aux abonnés aux événements spécifiés par le service spécifié.
    @return `false` si le `hService` spécifié n'est pas enregistré dans l'objet, sinon `true`.
    */
    bool addSubscriber(HSERVICE hService, HWND hWndReg, DWORD dwEventClass);
    bool removeSubscriber(HSERVICE hService, HWND hWndReg, DWORD dwEventClass);
public:
    /// Notifie tous les services des changements survenus aux lecteurs.
    void notifyChanges(const SCARD_READERSTATE& state, bool deviceChange);
};

#endif // PCSC_CENXFS_BRIDGE_ServiceContainer_H