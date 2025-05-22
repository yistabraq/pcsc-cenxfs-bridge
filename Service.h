#ifndef PCSC_CENXFS_BRIDGE_Service_H
#define PCSC_CENXFS_BRIDGE_Service_H

#pragma once

// CEN/XFS API -- Doit être en haut, car si placé ici,
#include <xfsapi.h>

#include "EventSupport.h"
#include "Settings.h"

#include "PCSC/ProtocolTypes.h"
#include "PCSC/Status.h"

#include "XFS/ReadFlags.h"
#include "XFS/ResetAction.h"

#include <string>
// Pour std::pair
#include <utility>
// CEN/XFS API -- Doit être en haut, car si placé ici,
// des erreurs de compilation étranges commencent à apparaître depuis winnt.h au moins dans MSVC 2005.
//#include <xfsapi.h>
// PC/CS API
#include <winscard.h>

class Manager;
class Service : public EventNotifier {
    Manager& pcsc;
    /// Handle du service XFS que cet objet représente
    HSERVICE hService;
    /// Handle de la carte avec laquelle l'opération sera effectuée.
    SCARDHANDLE hCard;
    /// Protocole utilisé par la carte.
    PCSC::ProtocolTypes mActiveProtocol;
    /// Nom du lecteur dont les notifications sont traitées par ce fournisseur de services.
    /// Peut être explicitement spécifié dans les paramètres, ou rempli au moment de la détection
    /// de la carte dans n'importe quel lecteur disponible. Dans ce dernier cas, tant que
    /// la carte n'est pas retirée, tous les événements des autres lecteurs seront ignorés.
    std::string mBindedReaderName;
    /// Paramètres de ce service.
    Settings mSettings;
    /// Drapeau indiquant qu'après la création du service, il a déjà pris connaissance de l'état actuel
    /// des lecteurs. Lors de la création du service, ce drapeau est mis à `false`,
    /// et lors de la première notification des lecteurs, il est mis à `true`.
    bool mInited;
    // Cette classe créera des objets de cette classe en appelant le constructeur.
    friend class ServiceContainer;
private:
    /** Ouvre la carte spécifiée pour l'opération.
    @param pcsc Gestionnaire de ressources du sous-système PC/SC.
    @param hService Handle attribué au service par le gestionnaire XFS.
    @param settings Paramètres du service XFS.
    */
    Service(Manager& pcsc, HSERVICE hService, const Settings& settings);
public:
    ~Service();

    PCSC::Status open(const char* readerName);
    PCSC::Status close();

    PCSC::Status lock();
    PCSC::Status unlock();

    inline void setTraceLevel(DWORD level) { mSettings.traceLevel = level; }
    /** Cette méthode est appelée lors de tout changement de lecteur et lors du changement du nombre de lecteurs.
    @param state
        Informations sur l'état actuel du lecteur modifié.
    */
    void notify(const SCARD_READERSTATE& state, bool deviceChange);
    /** Vérifie que le service attend des messages de ce lecteur. */
    bool match(const SCARD_READERSTATE& state, bool deviceChange);
public:// Fonctions appelées dans WFPGetInfo
    std::pair<WFSIDCSTATUS*, PCSC::Status> getStatus();
    std::pair<WFSIDCCAPS*, PCSC::Status> getCaps() const;
public:// Fonctions appelées dans WFPExecute
    /** Démarre l'opération d'attente de l'insertion d'une carte dans le lecteur. Dès que la carte
        est insérée dans le lecteur, un message `WFS_EXEE_IDC_MEDIAINSERTED` est généré,
        puis un message `WFS_EXECUTE_COMPLETE` avec le résultat `WFS_SUCCESS`.
    @par
        Si la carte n'apparaît pas dans le lecteur pendant le temps `dwTimeOut`, alors un message
        `WFS_EXECUTE_COMPLETE` avec le résultat `WFS_ERR_TIMEOUT` est généré.
    @par
        Si l'opération d'attente a été annulée, alors un message
        `WFS_EXECUTE_COMPLETE` avec le résultat `WFS_ERR_CANCELED` est généré.

    @param dwTimeOut
        Délai d'attente pour l'apparition de la carte dans le lecteur.
    @param hWnd
        Fenêtre à laquelle le message `WFS_EXECUTE_COMPLETE` avec le résultat
        de l'exécution de la fonction sera livré.
    @param ReqID
        Numéro de suivi pour suivre la requête, passé dans le message
        `WFS_EXECUTE_COMPLETE`.
    @param forRead
        Liste des données à lire. Seul `WFS_IDC_CHIP` est réellement lu,
        pour tous les autres types, un indicateur que la valeur n'a pas pu être lue est retourné.
    */
    void asyncRead(DWORD dwTimeOut, HWND hWnd, REQUESTID ReqID, XFS::ReadFlags forRead);

    std::pair<DWORD, BYTE*> readATR() const;
    WFSIDCCARDDATA* readChip() const;
    WFSIDCCARDDATA* readTrack2() const;
    WFSIDCCARDDATA** wrap(WFSIDCCARDDATA* iccData, XFS::ReadFlags forRead) const;

    /** Effectue la transmission de données du paramètre d'entrée vers la puce et reçoit une réponse de celle-ci.

    @param input
        Tampon reçu du sous-système XFS, contenant les paramètres du protocole et les données transmises.

    @return
        Une paire contenant le tampon à transmettre à l'application (elle gère la libération de la mémoire)
        et le statut d'exécution de la commande.
    */
    std::pair<WFSIDCCHIPIO*, PCSC::Status> transmit(const WFSIDCCHIPIO* input) const;
    /// Effectue la réinitialisation de la puce.
    std::pair<WFSIDCCHIPPOWEROUT*, PCSC::Status> reset(XFS::ResetAction action) const;
public:// Fonctions de service
    inline HSERVICE handle() const { return hService; }
    inline const Settings& settings() const { return mSettings; }
    inline const std::string& bindedReader() const { return mBindedReaderName; }
};

#endif // PCSC_CENXFS_BRIDGE_Service_H