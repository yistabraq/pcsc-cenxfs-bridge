#ifndef PCSC_CENXFS_BRIDGE_Task_H
#define PCSC_CENXFS_BRIDGE_Task_H

#pragma once

#include <boost/chrono/chrono.hpp>

#include <boost/shared_ptr.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/lock_guard.hpp>

// Conteneur pour stocker les tâches de lecture de carte.
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>

// PC/CS API -- pour SCARD_READERSTATE
#include <winscard.h>
// Définitions pour les lecteurs de cartes (Identification card unit (IDC)) --
// pour REQUESTID, HSERVICE et WFS_ERR_*
#include <XFSIDC.h>

namespace mi = boost::multi_index;
namespace bc = boost::chrono;

class Service;

/** Classe pour stocker les informations nécessaires à l'envoi des événements EXECUTE après
    leur occurrence à la fenêtre qui a été spécifiée comme destinataire lors de l'appel
    de la fonction `WFPExecute`.
*/
class Task {
public:
    /// Moment où le timeout de cette tâche expire.
    bc::steady_clock::time_point deadline;
    /// Service qui a créé cette tâche.
    Service& mService;
    /// Fenêtre qui recevra la notification de fin de tâche.
    HWND hWnd;
    /// Numéro de suivi de cette tâche qui sera fourni dans la notification à la fenêtre `hWnd`.
    /// Doit être unique pour chaque tâche.
    REQUESTID ReqID;
public:
    typedef boost::shared_ptr<Task> Ptr;
public:
    Task(bc::steady_clock::time_point deadline, Service& service, HWND hWnd, REQUESTID ReqID)
        : deadline(deadline), mService(service), hWnd(hWnd), ReqID(ReqID) {}
    inline bool operator<(const Task& other) const {
        return deadline < other.deadline;
    }
    inline bool operator==(const Task& other) const {
        return &mService == &other.mService && ReqID == other.ReqID;
    }
public:
    /** Vérifie si l'événement spécifié est celui attendu par cette tâche.
        Si la fonction retourne `true`, alors cette tâche sera considérée comme terminée
        et sera exclue de la liste des tâches enregistrées. Si la tâche nécessite
        d'effectuer des actions en cas de succès, cela doit être fait ici.

    @param state
        Données de l'état modifié.
    @param deviceChange
        Si `true`, alors le changement concerne une modification du nombre d'appareils, et non
        de la carte dans l'appareil.

    @return
        `true` si la tâche a attendu l'événement qui l'intéresse et doit être supprimée
        de la file d'attente des tâches, sinon `false`.
    */
    virtual bool match(const SCARD_READERSTATE& state, bool deviceChange) const = 0;
    /// Notifie l'auditeur XFS de la fin de l'attente.
    /// @param result Code de réponse pour la fin.
    void complete(HRESULT result) const;
    /// Appelé si la requête a été annulée par un appel à WFPCancelAsyncRequest.
    inline void cancel() const { complete(WFS_ERR_CANCELED); }
    inline void timeout() const { complete(WFS_ERR_TIMEOUT); }
    HSERVICE serviceHandle() const;
};
/// Contient la liste des tâches et les méthodes pour leur ajout, annulation et traitement thread-safe.
class TaskContainer {
    typedef mi::multi_index_container<
        Task::Ptr,
        mi::indexed_by<
            // Tri par CardWaitTask::operator< -- par temps de deadline, pour sélectionner
            // les tâches dont le timeout est atteint. Plusieurs tâches peuvent se terminer
            // au même moment, donc l'index n'est pas unique.
            mi::ordered_non_unique<mi::identity<Task> >,
            // Tri par less<REQUESTID> sur ReqID -- pour supprimer les tâches annulées,
            // mais ReqID est unique dans les limites du service.
            mi::ordered_unique<mi::composite_key<
                Task,
                BOOST_MULTI_INDEX_CONST_MEM_FUN(Task, HSERVICE, serviceHandle),
                mi::member<Task, REQUESTID, &Task::ReqID>
            > >
        >
    > TaskList;
private:
    /// Liste des tâches, ordonnée par temps de deadline croissant.
    /// Plus le deadline est proche, plus la tâche est proche du début de la liste.
    TaskList tasks;
    /// Mutex pour protéger `tasks` contre les modifications simultanées.
    mutable boost::recursive_mutex tasksMutex;
public:
    /// À la destruction du conteneur, toutes les tâches sont annulées.
    ~TaskContainer();
public:
    /** Ajoute une tâche à la file d'attente, retourne `true` si la tâche est la première dans la file
        et le deadline doit être ajusté pour ne pas manquer le deadline de cette tâche.
    @param task
        Nouvelle tâche.

    @return
        Si le deadline de la nouvelle tâche se trouve devant toutes les autres tâches, alors la méthode retourne
        `true`. Cela signifie qu'il faut mettre à jour le timeout d'attente des événements des lecteurs,
        pour ne pas manquer le timeout de cette tâche. Si le temps du deadline le plus proche ne change pas,
        retourne `false`.
    */
    bool addTask(const Task::Ptr& task);
    /** Annule la tâche créée par le service spécifié et ayant le numéro de suivi
        spécifié.
    @param hService
        Service qui a mis la tâche en file d'attente pour exécution et qui souhaite maintenant l'annuler.
    @param ReqID
        Numéro de la tâche, attribué par le gestionnaire XFS.

    @return
        `true` si la tâche a été trouvée (dans ce cas elle sera annulée), sinon `false`,
        ce qui signifie qu'aucune tâche avec ces paramètres n'existe dans la file d'attente.
    */
    bool cancelTask(HSERVICE hService, REQUESTID ReqID);

    /// Calcule le timeout jusqu'au prochain deadline de manière thread-safe.
    DWORD getTimeout() const;
    /** Supprime de la liste toutes les tâches dont le temps de deadline est antérieur ou égal à celui spécifié
        et signale aux auditeurs enregistrés dans la tâche l'occurrence du timeout.
    @param now
        Temps pour lequel on considère si le timeout est atteint ou non.
    */
    void processTimeouts(bc::steady_clock::time_point now);
    /** Notifie toutes les tâches d'un changement dans le lecteur. En conséquence, certaines tâches peuvent se terminer.
    @param state
        État du lecteur modifié, y compris cela peut être un changement
        du nombre de lecteurs.
    */
    void notifyChanges(const SCARD_READERSTATE& state, bool deviceChange);
};
#endif // PCSC_CENXFS_BRIDGE_Task_H