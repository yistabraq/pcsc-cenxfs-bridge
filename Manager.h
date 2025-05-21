#ifndef PCSC_CENXFS_BRIDGE_Manager_H
#define PCSC_CENXFS_BRIDGE_Manager_H

#pragma once

#include "ReaderChangesMonitor.h"
#include "ServiceContainer.h"
#include "Task.h"

#include "PCSC/Context.h"
#include "PCSC/Status.h"

#include "XFS/Result.h"

#include <vector>

#include <boost/chrono/chrono.hpp>

// PC/CS API
#include <winscard.h>
// Définitions pour les lecteurs de cartes (Identification card unit (IDC))
#include <XFSIDC.h>

// Liaison avec la bibliothèque d'implémentation de la norme PC/SC dans Windows
#pragma comment(lib, "winscard.lib")

class Service;
class Settings;
/** Classe qui, dans son constructeur, initialise le sous-système PC/SC, et dans son destructeur, le ferme.
    Il est nécessaire de créer exactement une instance de cette classe lors du chargement de la DLL et de la détruire
    lors du déchargement. La manière la plus simple de le faire est de déclarer une variable globale de cette classe.
*/
class Manager : public PCSC::Context {
private:
    // L'ordre des champs est important, car les objets
    // déclarés ci-dessous seront détruits en premier. Il est nécessaire en premier lieu
    // de terminer le thread d'interrogation des changements, puis d'envoyer des notifications
    // d'annulation de toutes les tâches et enfin de supprimer tous les services.

    /// Liste des services ouverts pour l'interaction avec le système XFS.
    ServiceContainer services;
    /// Conteneur gérant les tâches asynchrones pour obtenir des données de la carte.
    TaskContainer tasks;
    /// Objet pour surveiller l'état des lecteurs et envoyer des notifications
    /// lorsque l'état change. À la destruction, il arrête l'attente des changements.
    ReaderChangesMonitor readerChangesMonitor;
public:
    /// Ouvre une connexion au gestionnaire du sous-système PC/SC.
    Manager();
public:// Gestion des services
    /** Vérifie que le handle de service spécifié est un handle de carte valide. */
    inline bool isValid(HSERVICE hService) const { return services.isValid(hService); }
    /** @return true si aucun service n'est enregistré dans le gestionnaire. */
    inline bool isEmpty() const { return services.isEmpty(); }

    Service& create(HSERVICE hService, const Settings& settings);
    inline Service& get(HSERVICE hService) { return services.get(hService); }
    inline void remove(HSERVICE hService) { services.remove(hService); }
public:// Abonnement aux événements et génération d'événements
    /** Ajoute la fenêtre spécifiée aux abonnés aux événements spécifiés par le service spécifié.
    @return `false` si le `hService` spécifié n'est pas enregistré dans l'objet, sinon `true`.
    */
    inline bool addSubscriber(HSERVICE hService, HWND hWndReg, DWORD dwEventClass) {
        return services.addSubscriber(hService, hWndReg, dwEventClass);
    }
    inline bool removeSubscriber(HSERVICE hService, HWND hWndReg, DWORD dwEventClass) {
        return services.removeSubscriber(hService, hWndReg, dwEventClass);
    }
public:// Gestion des tâches
    void addTask(const Task::Ptr& task);
    /** Annule la tâche avec le numéro de suivi spécifié, retourne `true` si une tâche avec ce
        numéro existait dans la liste, sinon `false`.
    @param hService
        Service XFS pour lequel la tâche est annulée.
    @param ReqID
        Identifiant unique de la tâche à annuler. Il n'y a pas deux tâches avec le
        même identifiant dans la liste pour un même `hService`.

    @return
        `true` si une tâche avec ce numéro existait dans la liste, sinon `false`.
    */
    bool cancelTask(HSERVICE hService, REQUESTID ReqID);
private:// Fonctions pour utiliser ReaderChangesMonitor
    friend class ReaderChangesMonitor;
    /// @copydoc TaskContainer::getTimeout
    inline DWORD getTimeout() const { return tasks.getTimeout(); }
    /// @copydoc TaskContainer::processTimeouts
    inline void processTimeouts(boost::chrono::steady_clock::time_point now) { tasks.processTimeouts(now); }
    /// @copydoc TaskContainer::notifyChanges
    void notifyChanges(const SCARD_READERSTATE& state, bool deviceChange);
};

#endif // PCSC_CENXFS_BRIDGE_Manager_H