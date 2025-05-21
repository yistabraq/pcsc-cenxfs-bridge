#ifndef PCSC_CENXFS_BRIDGE_EventSupport_H
#define PCSC_CENXFS_BRIDGE_EventSupport_H

#pragma once

#include "XFS/Result.h"

#include <cassert>
#include <set>
#include <vector>
// Pour std::pair
#include <utility>
// Pour HWND et DWORD
#include <windef.h>

class EventSubscriber {
    /// Fenêtre qui doit recevoir les événements spécifiés.
    HWND hWnd;
    /// Masque de bits des événements activés pour la réception :
    ///   SERVICE_EVENTS
    ///   USER_EVENTS
    ///   SYSTEM_EVENTS
    ///   EXECUTE_EVENTS
    DWORD mask;

    friend class EventNotifier;
public:
    /*enum Event {
        /// Changement de l'état du périphérique (par exemple, disponibilité).
        Service = SERVICE_EVENTS,
        /// Le périphérique nécessite une attention de l'utilisateur, par exemple, le remplacement du papier dans une imprimante.
        User    = USER_EVENTS,
        /// Événements matériels, par exemple, erreurs matérielles, manque d'espace disque, incompatibilité de version.
        System  = SYSTEM_EVENTS,
        Execute = EXECUTE_EVENTS,
    };
    typedef Flags<Event> EventFlags;*/
public:
    EventSubscriber(HWND hWnd, DWORD mask) : hWnd(hWnd), mask(mask) {}
    bool operator==(const EventSubscriber& other) const {
        return hWnd == other.hWnd;
    }
    bool operator<(const EventSubscriber& other) const {
        return hWnd < other.hWnd;
    }
public:
    /// Ajoute un nouvel événement suivi au masque.
    void add(DWORD event) {
        mask |= event;
    }
    /// Supprime la classe d'événements spécifiée du masque.
    /// @return `true` si après la suppression il n'y a plus d'abonnement à aucun événement, sinon `false`.
    bool remove(DWORD event) {
        mask &= ~event;
        return mask == 0;
    }
    void notify(DWORD event, XFS::Result result) const {
        if (mask & event) {
            result.send(hWnd, event);
        }
    }
};
class EventNotifier {
    typedef std::set<EventSubscriber> SubscriberList;
    typedef std::pair<SubscriberList::iterator, bool> InsertResult;
private:
    SubscriberList subscribers;
public:
    void add(HWND hWnd, DWORD event) {
        assert(hWnd != NULL && "Attempt to subscribe NULL window to events");
        InsertResult r = subscribers.insert(EventSubscriber(hWnd, event));
        // Si l'élément spécifié existait déjà dans la carte, il ne sera pas remplacé,
        // et nous n'en avons pas besoin. Au lieu de cela, nous devons mettre à jour l'élément existant.
        if (r.second) {
            // La conversion est sûre, car la clé pour std::set ne change pas.
            const_cast<EventSubscriber&>(*r.first).add(event);
        }
    }
    void remove(HWND hWnd, DWORD event) {
        // NULL signifie que la désinscription se fait pour toutes les fenêtres
        if (hWnd == NULL) {
            std::vector<HWND> forRemove;
            for (SubscriberList::iterator it = subscribers.begin(); it != subscribers.end(); ++it) {
                // Supprime la classe d'événements. Si c'était la dernière classe d'événements intéressée,
                // supprime également l'abonné. Si 0 est spécifié, la désinscription se fait pour toutes les classes d'événements.
                if (event == 0 || const_cast<EventSubscriber&>(*it).remove(event)) {
                    // Pour C++11, on aurait pu utiliser it = subscribers.erase(it);
                    // Mais on préfère rester dans le cadre de C++03
                    forRemove.push_back(it->hWnd);
                }
            }
            for (std::vector<HWND>::const_iterator it = forRemove.begin(); it != forRemove.end(); ++it) {
                // Le deuxième paramètre du constructeur n'est pas important.
                subscribers.erase(EventSubscriber(*it, 0));
            }
        } else {
            SubscriberList::iterator it = subscribers.find(EventSubscriber(hWnd, event));
            if (it != subscribers.end()) {
                // Supprime la classe d'événements. Si c'était la dernière classe d'événements intéressée,
                // supprime également l'abonné. Si 0 est spécifié, la désinscription se fait pour toutes les classes d'événements.
                if (event == 0 || const_cast<EventSubscriber&>(*it).remove(event)) {
                    subscribers.erase(it);
                }
            }
        }
    }
    /// Supprime tous les abonnés aux événements.
    void clear() {
        subscribers.clear();
    }
    /** Notifie tous les abonnés de l'événement spécifié.
    @param resultGenerator Fonction qui doit retourner un résultat de type XFS::Result.
           Cette fonction est appelée pour chaque abonné à l'événement.
    */
    template<class F>
    void notify(DWORD event, F resultGenerator) const {
        SubscriberList::const_iterator end = subscribers.end();
        for (SubscriberList::const_iterator it = subscribers.begin(); it != subscribers.end(); ++it) {
            it->notify(event, resultGenerator());
        }
    }
};

#endif // PCSC_CENXFS_BRIDGE_EventSupport_H