#include "Manager.h"
#include "Service.h"
#include "Settings.h"

#include "PCSC/Status.h"

#include "XFS/Result.h"

// Pour strncpy.
#include <cstring>
// Pour std::size_t.
#include <cstddef>
// Pour std::pair
#include <utility>
#include <vector>

// CEN/XFS API -- Notre fichier, car l'original est destiné à être utilisé
// par les clients, et nous devons exporter des fonctions, pas les importer.
#include "xfsspi.h"
// Définitions pour les lecteurs de cartes (Identification card unit (IDC))
#include <XFSIDC.h>

// Liaison avec la bibliothèque d'implémentation de la norme PC/SC dans Windows
#pragma comment(lib, "winscard.lib")
// Liaison avec la bibliothèque d'implémentation XFS
#pragma comment(lib, "msxfs.lib")
// Liaison avec la bibliothèque de support des fonctions de configuration XFS
#pragma comment(lib, "xfs_conf.lib")
// Liaison avec la bibliothèque pour l'utilisation des fonctions de fenêtre (PostMessage)
#pragma comment(lib, "user32.lib")

#define MAKE_VERSION(major, minor) (((major) << 8) | (minor))

#define DLL_VERSION \
    "PC/SC to CEN/XFS bridge\n" \
    "https://bitbucket.org/Mingun/pcsc-cenxfs-bridge\n" \
    "Build: Date: " __DATE__ ", time: " __TIME__

template<std::size_t N1, std::size_t N2>
void safecopy(char (&dst)[N1], const char (&src)[N2]) {
    std::strncpy(dst, src, N1 < N2 ? N1 : N2);
}

/** Lors du chargement de la DLL, les constructeurs de tous les objets globaux seront appelés et une
    connexion au sous-système PC/SC sera établie. Lors du déchargement de la DLL, les destructeurs des
    objets globaux seront appelés et la connexion se fermera automatiquement.
*/
Manager pcsc;
extern "C" {
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

/** Tente de déterminer la présence d'un lecteur portant le nom spécifié dans les paramètres (clé ReaderName).
    Si cette clé n'est pas spécifiée, se lie au premier lecteur trouvé. S'il n'y a actuellement
    aucun lecteur détecté dans le système, attend le temps spécifié dans `dwTimeOut` jusqu'à ce qu'il soit connecté.

@message WFS_OPEN_COMPLETE

@param hService Handle de session auquel il faut associer le handle de la carte en cours d'ouverture.
@param lpszLogicalName Nom du service logique, configuré dans le registre HKLM\<user>\XFS\LOGICAL_SERVICES
@param hApp Handle de l'application, créé par l'appel à WFSCreateAppHandle.
@param lpszAppID Identifiant de l'application. Peut être `NULL`.
@param dwTraceLevel 
@param dwTimeOut Délai d'attente (en ms) pendant lequel le service doit être ouvert. `WFS_INDEFINITE_WAIT`,
       si aucun délai n'est requis.
@param hWnd Fenêtre qui doit recevoir le message de fin de l'opération asynchrone.
@param ReqID Identifiant de la requête qui doit être passé à la fenêtre `hWnd` à la fin de l'opération.
@param hProvider Handle de ce fournisseur de services. Peut être passé, par exemple, à la fonction WFMReleaseDLL.
@param dwSPIVersionsRequired Plage de versions du fournisseur requises. 0x00112233 signifie la plage
       de versions [0.11; 22.33].
@param lpSPIVersion Informations sur la connexion ouverte (paramètre de sortie).
@param dwSrvcVersionsRequired Version requise spécifique au service (lecteur de carte) du fournisseur.
       0x00112233 signifie la plage de versions [0.11; 22.33].
@param lpSrvcVersion Informations sur la connexion ouverte, spécifiques au type de fournisseur (lecteur de carte)
       (paramètre de sortie).
@return Toujours `WFS_SUCCESS`.
*/
HRESULT SPI_API WFPOpen(HSERVICE hService, LPSTR lpszLogicalName, 
                        HAPP hApp, LPSTR lpszAppID, 
                        DWORD dwTraceLevel, DWORD dwTimeOut, 
                        HWND hWnd, REQUESTID ReqID,
                        HPROVIDER hProvider, 
                        DWORD dwSPIVersionsRequired, LPWFSVERSION lpSPIVersion, 
                        DWORD dwSrvcVersionsRequired, LPWFSVERSION lpSrvcVersion
) {
    // Retourne les versions supportées.
    if (lpSPIVersion != NULL) {
        // Version du gestionnaire XFS qui sera utilisée. Comme nous supportons toutes les versions,
        // il s'agit simplement de la version maximale demandée (elle se trouve dans le mot supérieur).
        lpSPIVersion->wVersion = HIWORD(dwSPIVersionsRequired);
        // Version minimale et maximale que nous supportons
        //TODO: Préciser la version minimale supportée !
        lpSPIVersion->wLowVersion  = MAKE_VERSION(0, 0);
        lpSPIVersion->wHighVersion = MAKE_VERSION(255, 255);
        safecopy(lpSPIVersion->szDescription, DLL_VERSION);
    }
    if (lpSrvcVersion != NULL) {
        // Version du gestionnaire XFS qui sera utilisée. Comme nous supportons toutes les versions,
        // il s'agit simplement de la version maximale demandée (elle se trouve dans le mot supérieur).
        lpSrvcVersion->wVersion = HIWORD(dwSrvcVersionsRequired);
        // Version minimale et maximale que nous supportons
        //TODO: Préciser la version minimale supportée !
        lpSrvcVersion->wLowVersion  = MAKE_VERSION(0, 0);
        lpSrvcVersion->wHighVersion = MAKE_VERSION(255, 255);
        safecopy(lpSrvcVersion->szDescription, DLL_VERSION);
    }

    pcsc.create(hService, Settings(lpszLogicalName, dwTraceLevel));
    XFS::Result(ReqID, hService, WFS_SUCCESS).send(hWnd, WFS_OPEN_COMPLETE);

    // Codes de fin possibles pour une requête asynchrone (d'autres peuvent également être retournés)
    // WFS_ERR_CANCELED                La requête a été annulée par WFSCancelAsyncRequest.
    // WFS_ERR_INTERNAL_ERROR          Une incohérence interne ou une autre erreur inattendue s'est produite dans le sous-système XFS.
    // WFS_ERR_TIMEOUT                 L'intervalle de délai d'attente a expiré.
    // WFS_ERR_VERSION_ERROR_IN_SRVC   Dans le service, une incompatibilité de version de deux modules s'est produite.

    // Codes de fin possibles pour la fonction :
    // WFS_ERR_CONNECTION_LOST       La connexion au service est perdue.
    // WFS_ERR_INTERNAL_ERROR        Une incohérence interne ou une autre erreur inattendue s'est produite dans le sous-système XFS.
    // WFS_ERR_INVALID_HSERVICE      Le paramètre hService n'est pas un handle de service valide.
    // WFS_ERR_INVALID_HWND          Le paramètre hWnd n'est pas un handle de fenêtre valide.
    // WFS_ERR_INVALID_POINTER       Un paramètre pointeur ne pointe pas vers une mémoire accessible.
    // WFS_ERR_INVALID_TRACELEVEL    Le paramètre dwTraceLevel ne correspond pas à un niveau de trace valide ou à un ensemble de niveaux.
    // WFS_ERR_SPI_VER_TOO_HIGH      La plage de versions du support SPI XFS demandée par le gestionnaire XFS est supérieure à celle supportée par ce fournisseur de services particulier.
    // WFS_ERR_SPI_VER_TOO_LOW       La plage de versions du support SPI XFS demandée par le gestionnaire XFS est inférieure à celle supportée par ce fournisseur de services particulier.
    // WFS_ERR_SRVC_VER_TOO_HIGH     La plage de versions du support d'interface spécifique au service demandée par l'application est supérieure à celle supportée par le fournisseur de services pour le service logique en cours d'ouverture.
    // WFS_ERR_SRVC_VER_TOO_LOW      La plage de versions du support d'interface spécifique au service demandée par l'application est inférieure à celle supportée par le fournisseur de services pour le service logique en cours d'ouverture.
    // WFS_ERR_VERSION_ERROR_IN_SRVC Dans le service, une incompatibilité de version de deux modules s'est produite.

    return WFS_SUCCESS;
}
/** Termine une session (série de requêtes vers un service, initiée par l'appel à la fonction SPI WFPOpen)
    entre le gestionnaire XFS et le fournisseur de services spécifié.
@par
    Si le fournisseur est bloqué par un appel à WFPLock, il sera automatiquement débloqué et désinscrit de
    tous les événements (si cela n'a pas été fait précédemment par un appel à WFPDeregister).
@message WFS_CLOSE_COMPLETE

@param hService Fournisseur de services en cours de fermeture.
@param hWnd Fenêtre qui doit recevoir le message de fin de l'opération asynchrone.
@param ReqId Identifiant de la requête qui doit être passé à la fenêtre `hWnd` à la fin de l'opération.
*/
HRESULT SPI_API WFPClose(HSERVICE hService, HWND hWnd, REQUESTID ReqID) {
    if (!pcsc.isValid(hService))
        return WFS_ERR_INVALID_HSERVICE;
    pcsc.remove(hService);
    XFS::Result(ReqID, hService, WFS_SUCCESS).send(hWnd, WFS_CLOSE_COMPLETE);

    // Codes de fin possibles pour une requête asynchrone (d'autres peuvent également être retournés)
    // WFS_ERR_CANCELED La requête a été annulée par WFSCancelAsyncRequest.
    // WFS_ERR_INTERNAL_ERROR Une incohérence interne ou une autre erreur inattendue s'est produite dans le sous-système XFS.

    // Codes de fin possibles pour la fonction :
    // WFS_ERR_CONNECTION_LOST La connexion au service est perdue.
    // WFS_ERR_INTERNAL_ERROR Une incohérence interne ou une autre erreur inattendue s'est produite dans le sous-système XFS.
    // WFS_ERR_INVALID_HSERVICE Le paramètre hService n'est pas un handle de service valide.
    // WFS_ERR_INVALID_HWND Le paramètre hWnd n'est pas un handle de fenêtre valide.
    return WFS_SUCCESS;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/** Abonne la fenêtre spécifiée aux événements des classes spécifiées par le fournisseur de services spécifié.

@message WFS_REGISTER_COMPLETE

@param hService Service dont les messages doivent être surveillés.
@param dwEventClass OR logique des classes de messages à surveiller. 0 signifie que
       l'abonnement se fait pour toutes les classes d'événements.
@param hWndReg Fenêtre qui recevra les événements des classes spécifiées.
@param hWnd Fenêtre qui doit recevoir le message de fin de l'opération asynchrone.
@param ReqID Identifiant de la requête qui doit être passé à la fenêtre `hWnd` à la fin de l'opération.
*/
HRESULT SPI_API WFPRegister(HSERVICE hService,  DWORD dwEventClass, HWND hWndReg, HWND hWnd, REQUESTID ReqID) {
    // Enregistre l'événement pour la fenêtre.
    if (!pcsc.addSubscriber(hService, hWndReg, dwEventClass)) {
        // Si le service n'est pas dans PC/SC, il est perdu.
        return WFS_ERR_INVALID_HSERVICE;
    }
    // L'ajout de l'abonné est toujours réussi.
    XFS::Result(ReqID, hService, WFS_SUCCESS).send(hWnd, WFS_REGISTER_COMPLETE);
    // Codes de fin possibles pour une requête asynchrone (d'autres peuvent également être retournés)
    // WFS_ERR_CANCELED        La requête a été annulée par WFSCancelAsyncRequest.
    // WFS_ERR_INTERNAL_ERROR  Une incohérence interne ou une autre erreur inattendue s'est produite dans le sous-système XFS.

    // Codes de fin possibles pour la fonction :
    // WFS_ERR_CONNECTION_LOST     La connexion au service est perdue.
    // WFS_ERR_INTERNAL_ERROR      Une incohérence interne ou une autre erreur inattendue s'est produite dans le sous-système XFS.
    // WFS_ERR_INVALID_EVENT_CLASS Le paramètre dwEventClass spécifie une ou plusieurs classes d'événements non supportées par le service.
    // WFS_ERR_INVALID_HSERVICE    Le paramètre hService n'est pas un handle de service valide.
    // WFS_ERR_INVALID_HWND        Le paramètre hWnd n'est pas un handle de fenêtre valide.
    // WFS_ERR_INVALID_HWNDREG     Le paramètre hWndReg n'est pas un handle de fenêtre valide.
    // WFS_ERR_NOT_REGISTERED      La fenêtre hWndReg spécifiée n'était pas enregistrée pour recevoir des messages pour aucune classe d'événements.
    return WFS_SUCCESS;
}
/** Interrompt la surveillance des classes de messages spécifiées par le fournisseur de services spécifié pour les fenêtres spécifiées.

@message WFS_DEREGISTER_COMPLETE

@param hService Service dont les messages ne doivent plus être surveillés.
@param dwEventClass OR logique des classes de messages qui ne doivent plus être surveillées. 0 signifie que la désinscription
       se fait pour toutes les classes d'événements.
@param hWndReg Fenêtre qui ne doit plus recevoir les messages spécifiés. NULL signifie que la désinscription
       se fait pour toutes les fenêtres.
@param hWnd Fenêtre qui doit recevoir le message de fin de l'opération asynchrone.
@param ReqID Identifiant de la requête qui doit être passé à la fenêtre `hWnd` à la fin de l'opération.
*/
HRESULT SPI_API WFPDeregister(HSERVICE hService, DWORD dwEventClass, HWND hWndReg, HWND hWnd, REQUESTID ReqID) {
    // Se désabonne des événements. Si personne n'a été supprimé, personne n'était enregistré.
    if (!pcsc.removeSubscriber(hService, hWndReg, dwEventClass)) {
        // Si le service n'est pas dans PC/SC, il est perdu.
        return WFS_ERR_INVALID_HSERVICE;
    }
    // La suppression de l'abonné est toujours réussie.
    XFS::Result(ReqID, hService, WFS_SUCCESS).send(hWnd, WFS_DEREGISTER_COMPLETE);
    // Codes de fin possibles pour une requête asynchrone (d'autres peuvent également être retournés)
    // WFS_ERR_CANCELED La requête a été annulée par WFSCancelAsyncRequest.

    // Codes de fin possibles pour la fonction :
    // WFS_ERR_CONNECTION_LOST     La connexion au service est perdue.
    // WFS_ERR_INTERNAL_ERROR      Une incohérence interne ou une autre erreur inattendue s'est produite dans le sous-système XFS.
    // WFS_ERR_INVALID_EVENT_CLASS Le paramètre dwEventClass spécifie une ou plusieurs classes d'événements non supportées par le service.
    // WFS_ERR_INVALID_HSERVICE    Le paramètre hService n'est pas un handle de service valide.
    // WFS_ERR_INVALID_HWND        Le paramètre hWnd n'est pas un handle de fenêtre valide.
    // WFS_ERR_INVALID_HWNDREG     Le paramètre hWndReg n'est pas un handle de fenêtre valide.
    // WFS_ERR_NOT_REGISTERED      La fenêtre hWndReg spécifiée n'était pas enregistrée pour recevoir des messages pour aucune classe d'événements.
    return WFS_SUCCESS;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/** Obtient un accès exclusif au périphérique.

@message WFS_LOCK_COMPLETE

@param hService Service auquel il faut obtenir un accès exclusif.
@param dwTimeOut Délai d'attente (en ms) pendant lequel l'accès doit être obtenu. `WFS_INDEFINITE_WAIT`,
       si aucun délai n'est requis.
@param hWnd Fenêtre qui doit recevoir le message de fin de l'opération asynchrone.
@param ReqID Identifiant de la requête qui doit être passé à la fenêtre `hWnd` à la fin de l'opération.
*/
HRESULT SPI_API WFPLock(HSERVICE hService, DWORD dwTimeOut, HWND hWnd, REQUESTID ReqID) {
    if (!pcsc.isValid(hService))
        return WFS_ERR_INVALID_HSERVICE;

    PCSC::Status st = pcsc.get(hService).lock();
    XFS::Result(ReqID, hService, st).send(hWnd, WFS_LOCK_COMPLETE);

    // Codes de fin possibles pour une requête asynchrone (d'autres peuvent également être retournés)
    // WFS_ERR_CANCELED        La requête a été annulée par WFSCancelAsyncRequest.
    // WFS_ERR_DEV_NOT_READY   La fonction a requis l'accès au périphérique, et le périphérique n'était pas prêt ou a expiré.
    // WFS_ERR_HARDWARE_ERROR  La fonction a requis l'accès au périphérique, et une erreur s'est produite sur le périphérique.
    // WFS_ERR_INTERNAL_ERROR  Une incohérence interne ou une autre erreur inattendue s'est produite dans le sous-système XFS.
    // WFS_ERR_SOFTWARE_ERROR  La fonction a requis l'accès aux informations de configuration, et une erreur s'est produite sur le logiciel.
    // WFS_ERR_TIMEOUT         L'intervalle de délai d'attente a expiré.

    // Codes de fin possibles pour la fonction :
    // WFS_ERR_CONNECTION_LOST    The connection to the service is lost.
    // WFS_ERR_INTERNAL_ERROR     An internal inconsistency or other unexpected error occurred in the XFS subsystem.
    // WFS_ERR_INVALID_HSERVICE   The hService parameter is not a valid service handle.
    // WFS_ERR_INVALID_HWND       The hWnd parameter is not a valid window handle.
    return WFS_SUCCESS;
}
/**
@message WFS_UNLOCK_COMPLETE

@param hService Service auquel il faut supprimer l'accès exclusif.
@param hWnd Fenêtre qui doit recevoir le message de fin de l'opération asynchrone.
@param ReqID Identifiant de la requête qui doit être passé à la fenêtre `hWnd` à la fin de l'opération.
*/
HRESULT SPI_API WFPUnlock(HSERVICE hService, HWND hWnd, REQUESTID ReqID) {
    if (!pcsc.isValid(hService))
        return WFS_ERR_INVALID_HSERVICE;

    PCSC::Status st = pcsc.get(hService).unlock();
    XFS::Result(ReqID, hService, st).send(hWnd, WFS_UNLOCK_COMPLETE);
    // Codes de fin possibles pour une requête asynchrone (d'autres peuvent également être retournés)
    // WFS_ERR_CANCELED        La requête a été annulée par WFSCancelAsyncRequest.
    // WFS_ERR_INTERNAL_ERROR  Une incohérence interne ou une autre erreur inattendue s'est produite dans le sous-système XFS.
    // WFS_ERR_NOT_LOCKED      Le service à déverrouiller n'est pas verrouillé sous le hService appelant.

    // Codes de fin possibles pour la fonction :
    // WFS_ERR_CONNECTION_LOST    The connection to the service is lost.
    // WFS_ERR_INTERNAL_ERROR     An internal inconsistency or other unexpected error occurred in the XFS subsystem.
    // WFS_ERR_INVALID_HSERVICE   The hService parameter is not a valid service handle.
    // WFS_ERR_INVALID_HWND       The hWnd parameter is not a valid window handle.

    return WFS_SUCCESS;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

/** Obtient diverses informations sur le fournisseur de services.

@message WFS_GETINFO_COMPLETE

@param hService Handle du fournisseur de services dont on veut obtenir des informations.
@param dwCategory Type d'informations demandées. Seules les types d'informations standard sont supportés.
@param pQueryDetails Pointeur vers des données supplémentaires pour la requête ou `NULL`, si elles n'existent pas.
@param dwTimeOut Délai avant de terminer l'opération.
@param hWnd Fenêtre qui doit recevoir le message de fin de l'opération asynchrone.
@param ReqID Identifiant de la requête qui doit être passé à la fenêtre `hWnd` à la fin de l'opération.
*/
HRESULT SPI_API WFPGetInfo(HSERVICE hService, DWORD dwCategory, LPVOID lpQueryDetails, DWORD dwTimeOut, HWND hWnd, REQUESTID ReqID) {
    if (!pcsc.isValid(hService))
        return WFS_ERR_INVALID_HSERVICE;
    // Pour IDC, seules ces constantes peuvent être demandées (WFS_INF_IDC_*)
    switch (dwCategory) {
        case WFS_INF_IDC_STATUS: {      // Pas de paramètres supplémentaires
            std::pair<WFSIDCSTATUS*, PCSC::Status> status = pcsc.get(hService).getStatus();
            // Obtention d'informations sur le lecteur est toujours réussie.
            XFS::Result(ReqID, hService, WFS_SUCCESS).attach(status.first).send(hWnd, WFS_GETINFO_COMPLETE);
            break;
        }
        case WFS_INF_IDC_CAPABILITIES: {// Pas de paramètres supplémentaires
            std::pair<WFSIDCCAPS*, PCSC::Status> caps = pcsc.get(hService).getCaps();
            XFS::Result(ReqID, hService, caps.second).attach(caps.first).send(hWnd, WFS_GETINFO_COMPLETE);
            break;
        }
        case WFS_INF_IDC_FORM_LIST:
        case WFS_INF_IDC_QUERY_FORM: {
            // Les formes ne sont pas supportées. La forme détermine où les données se trouvent sur les pistes.
            // Comme nous ne lisons ni n'écrivons pas les pistes, les formes ne sont pas supportées.
            return WFS_ERR_UNSUPP_COMMAND;
        }
        default:
            return WFS_ERR_INVALID_CATEGORY;
    }
    // Codes de fin possibles pour une requête asynchrone (d'autres peuvent également être retournés)
    // WFS_ERR_CANCELED        La requête a été annulée par WFSCancelAsyncRequest.
    // WFS_ERR_DEV_NOT_READY   La fonction a requis l'accès au périphérique, et le périphérique n'était pas prêt ou a expiré.
    // WFS_ERR_HARDWARE_ERROR  La fonction a requis l'accès au périphérique, et une erreur s'est produite sur le périphérique.
    // WFS_ERR_INTERNAL_ERROR  Une incohérence interne ou une autre erreur inattendue s'est produite dans le sous-système XFS.
    // WFS_ERR_INVALID_DATA    La structure de données passée en paramètre d'entrée contient des données non valides..
    // WFS_ERR_SOFTWARE_ERROR  La fonction a requis l'accès aux informations de configuration, et une erreur s'est produite sur le logiciel.
    // WFS_ERR_TIMEOUT         L'intervalle de délai d'attente a expiré.
    // WFS_ERR_USER_ERROR      Un utilisateur empêche le bon fonctionnement de l'appareil.
    // WFS_ERR_UNSUPP_DATA     La structure de données passée en paramètre d'entrée, bien que valide pour cette classe de service, n'est pas supportée par ce fournisseur de services ou l'appareil.

    // Codes de fin possibles pour la fonction :
    // WFS_ERR_CONNECTION_LOST    The connection to the service is lost.
    // WFS_ERR_INTERNAL_ERROR     An internal inconsistency or other unexpected error occurred in the XFS subsystem.
    // WFS_ERR_INVALID_CATEGORY   Le dwCategory émis n'est pas supporté par cette classe de service.
    // WFS_ERR_INVALID_HSERVICE   Le paramètre hService n'est pas un handle de service valide.
    // WFS_ERR_INVALID_HWND       Le paramètre hWnd n'est pas un handle de fenêtre valide.
    // WFS_ERR_INVALID_POINTER    Un paramètre pointeur ne pointe pas vers une mémoire accessible.
    // WFS_ERR_UNSUPP_CATEGORY    Le dwCategory émis, bien que valide pour cette classe de service, n'est pas supporté par ce fournisseur de services.

    return WFS_SUCCESS;
}
/** Envoie une commande pour exécuter le lecteur de carte.

@message WFS_EXECUTE_COMPLETE
@message WFS_EXEE_IDC_MEDIAINSERTED Généré lorsqu'une carte apparaît dans l'appareil.
@message WFS_SRVE_IDC_MEDIAREMOVED Généré lorsqu'une carte est retirée de l'appareil.
@message WFS_EXEE_IDC_INVALIDMEDIA
@message WFS_SRVE_IDC_MEDIADETECTED
@message WFS_EXEE_IDC_INVALIDTRACKDATA N'est jamais généré.
@message WFS_USRE_IDC_RETAINBINTHRESHOLD N'est jamais généré.

@param hService Handle du fournisseur de services qui doit exécuter la commande.
@param dwCommand Type de commande à exécuter.
@param lpCmdData Données pour la commande. Dépendent du type.
@param dwTimeOut Nombre de millisecondes pendant lesquelles la commande doit être exécutée, ou `WFS_INDEFINITE_WAIT`,
       si aucun délai n'est requis.
@param hWnd Fenêtre qui doit recevoir le message de fin de l'opération asynchrone.
@param ReqID Identifiant de la requête qui doit être passé à la fenêtre `hWnd` à la fin de l'opération.
*/
HRESULT SPI_API WFPExecute(HSERVICE hService, DWORD dwCommand, LPVOID lpCmdData, DWORD dwTimeOut, HWND hWnd, REQUESTID ReqID) {
    if (!pcsc.isValid(hService))
        return WFS_ERR_INVALID_HSERVICE;

    switch (dwCommand) {
        // Attente d'insertion d'une carte avec délai spécifié, lecture immédiate des pistes selon la forme,
        // transmise en paramètre. Comme nous ne savons pas lire les pistes, cette commande n'est pas supportée.
        case WFS_CMD_IDC_READ_TRACK: {
            if (lpCmdData == NULL) {
                return WFS_ERR_INVALID_POINTER;
            }
            LPSTR formName = (LPSTR)lpCmdData;
            return WFS_ERR_UNSUPP_COMMAND;
        }
        // Identique à la lecture des pistes.
        case WFS_CMD_IDC_WRITE_TRACK: {
            //WFSIDCWRITETRACK* input = (WFSIDCWRITETRACK*)lpCmdData;
            return WFS_ERR_UNSUPP_COMMAND;
        }
        // La commande demande au lecteur de retourner la carte. Comme notre lecteur ne peut rien faire
        // avec la carte, nous ne la supportons pas.
        case WFS_CMD_IDC_EJECT_CARD: {// Pas de paramètres d'entrée.
            // Kalignite essaie d'extraire la carte, ignorant le fait que nous lui disons que cette fonctionnalité
            // n'est pas supportée, et tombe en erreur fatale si nous lui disons qu'il demande quelque chose d'impossible,
            // bien que, selon la spécification, nous devons lui dire que cette fonctionnalité n'est pas supportée
            // par le code de réponse WFS_ERR_UNSUPP_COMMAND et nous avons le droit de ne pas la supporter.
            if (pcsc.get(hService).settings().workarounds.canEject) {
                XFS::Result(ReqID, hService, WFS_SUCCESS).eject().send(hWnd, WFS_EXECUTE_COMPLETE);
                return WFS_SUCCESS;
            }
            return WFS_ERR_UNSUPP_COMMAND;
        }
        // La commande capture la carte lue par le lecteur. Identique à la commande précédente.
        case WFS_CMD_IDC_RETAIN_CARD: {// Pas de paramètres d'entrée.
            return WFS_ERR_UNSUPP_COMMAND;
        }
        // La commande réinitialise le compteur de cartes capturées. Comme nous ne les capturons pas, nous ne les supportons pas.
        case WFS_CMD_IDC_RESET_COUNT: {// Pas de paramètres d'entrée.
            return WFS_ERR_UNSUPP_COMMAND;
        }
        // Définit la clé DES nécessaire pour le fonctionnement du module CIM86. Nous ne la supportons pas,
        // car nous n'avons pas de tel module.
        case WFS_CMD_IDC_SETKEY: {
            //WFSIDCSETKEY* = (WFSIDCSETKEY*)lpCmdData;
            return WFS_ERR_UNSUPP_COMMAND;
        }
        // Connecte le chip, le réinitialise et lit ATR (Answer To Reset).
        // Cette commande peut également être utilisée pour un "reset froid" du chip.
        // Cette commande ne doit pas être utilisée pour un chip permanentement connecté.
        //TODO: Le chip est permanent ou non?
        case WFS_CMD_IDC_READ_RAW_DATA: {
            if (lpCmdData == NULL) {
                return WFS_ERR_INVALID_POINTER;
            }
            // Masque binaire avec les données qui doivent être lues.
            XFS::ReadFlags readData = *((WORD*)lpCmdData);
            if (readData.value() & WFS_IDC_CHIP) {
                pcsc.get(hService).asyncRead(dwTimeOut, hWnd, ReqID, readData);
                return WFS_SUCCESS;
            }
            return WFS_ERR_UNSUPP_COMMAND;
        }
        // Attend le temps spécifié jusqu'à ce qu'une carte soit insérée, puis écrit les données sur la piste spécifiée.
        // Nous ne la supportons pas, car nous ne savons pas écrire des pistes.
        case WFS_CMD_IDC_WRITE_RAW_DATA: {
            //NULL-terminated array
            //WFSIDCCARDDATA** data = (WFSIDCCARDDATA**)lpCmdData;
            return WFS_ERR_UNSUPP_COMMAND;
        }
        // Envoie des données au chip et reçoit la réponse de celui-ci. Les données sont transparentes pour le fournisseur.
        case WFS_CMD_IDC_CHIP_IO: {
            if (lpCmdData == NULL) {
                return WFS_ERR_INVALID_POINTER;
            }
            const WFSIDCCHIPIO* data = (const WFSIDCCHIPIO*)lpCmdData;
            std::pair<WFSIDCCHIPIO*, PCSC::Status> result = pcsc.get(hService).transmit(data);
            XFS::Result(ReqID, hService, result.second).attach(result.first).send(hWnd, WFS_EXECUTE_COMPLETE);
            return WFS_SUCCESS;
        }
        // Déconnecte le chip.
        case WFS_CMD_IDC_RESET: {
            // Masque binaire. NULL signifie que le fournisseur décide ce que faire.
            WORD wResetIn = lpCmdData ? *((WORD*)lpCmdData) : WFS_IDC_NOACTION;
            //TODO: Implémenter la commande WFS_CMD_IDC_RESET
            return WFS_ERR_UNSUPP_COMMAND;
        }
        case WFS_CMD_IDC_CHIP_POWER: {
            if (lpCmdData == NULL) {
                return WFS_ERR_INVALID_POINTER;
            }
            WORD wChipPower = *((WORD*)lpCmdData);
            std::pair<WFSIDCCHIPPOWEROUT*, PCSC::Status> result = pcsc.get(hService).reset(wChipPower);
            XFS::Result(ReqID, hService, result.second).attach(result.first).send(hWnd, WFS_EXECUTE_COMPLETE);
            return WFS_SUCCESS;
        }
        // Analyse le résultat, précédemment retourné par la commande WFS_CMD_IDC_READ_RAW_DATA. Comme nous ne la supportons pas, nous ne la supportons pas.
        case WFS_CMD_IDC_PARSE_DATA: {
            // WFSIDCPARSEDATA* parseData = (WFSIDCPARSEDATA*)lpCmdData;
            return WFS_ERR_UNSUPP_COMMAND;
        }
        default: {
            // Toutes les autres commandes sont invalides.
            return WFS_ERR_INVALID_COMMAND;
        }
    }// switch (dwCommand)
    // Codes de fin possibles pour une requête asynchrone (d'autres peuvent également être retournés)
    // WFS_ERR_CANCELED        La requête a été annulée par WFSCancelAsyncRequest.
    // WFS_ERR_DEV_NOT_READY   La fonction a requis l'accès au périphérique, et le périphérique n'était pas prêt ou a expiré.
    // WFS_ERR_HARDWARE_ERROR  La fonction a requis l'accès au périphérique, et une erreur s'est produite sur le périphérique.
    // WFS_ERR_INVALID_DATA    La structure de données passée en paramètre d'entrée contient des données non valides..
    // WFS_ERR_INTERNAL_ERROR  Une incohérence interne ou une autre erreur inattendue s'est produite dans le sous-système XFS.
    // WFS_ERR_LOCKED          Le service est verrouillé sous un hService différent.
    // WFS_ERR_SOFTWARE_ERROR  La fonction a requis l'accès aux informations de configuration, et une erreur s'est produite sur le logiciel.
    // WFS_ERR_TIMEOUT         L'intervalle de délai d'attente a expiré.
    // WFS_ERR_USER_ERROR      Un utilisateur empêche le bon fonctionnement de l'appareil.
    // WFS_ERR_UNSUPP_DATA     La structure de données passée en paramètre d'entrée, bien que valide pour cette classe de service, n'est pas supportée par ce fournisseur de services ou l'appareil.

    // Codes de fin possibles pour la fonction :
    // WFS_ERR_CONNECTION_LOST    The connection to the service is lost.
    // WFS_ERR_INTERNAL_ERROR     An internal inconsistency or other unexpected error occurred in the XFS subsystem.
    // WFS_ERR_INVALID_COMMAND    The dwCommand issued is not supported by this service class.
    // WFS_ERR_INVALID_HSERVICE   The hService parameter is not a valid service handle.
    // WFS_ERR_INVALID_HWND       The hWnd parameter is not a valid window handle.
    // WFS_ERR_INVALID_POINTER    A pointer parameter does not point to accessible memory.
    // WFS_ERR_UNSUPP_COMMAND     The dwCommand issued, although valid for this service class, is not supported by this service provider.
    return WFS_ERR_INTERNAL_ERROR;
}
/** Annule une requête asynchrone spécifiée (ou toutes pour un service spécifié) avant qu'elle ne se termine.
    Toutes les requêtes qui n'ont pas pu être exécutées à ce moment seront terminées avec le code `WFS_ERR_CANCELED`.

    L'annulation des requêtes n'est pas supportée.
@param hService
@param ReqID Identifiant de la requête à annuler ou `NULL`, si toutes les requêtes doivent être annulées
       pour le service spécifié `hService`.
*/
HRESULT SPI_API WFPCancelAsyncRequest(HSERVICE hService, REQUESTID ReqID) {
    if (!pcsc.isValid(hService))
        return WFS_ERR_INVALID_HSERVICE;
    if (!pcsc.cancelTask(hService, ReqID)) {
        return WFS_ERR_INVALID_REQ_ID;
    }
    // Codes de fin possibles pour la fonction :
    // WFS_ERR_CONNECTION_LOST  The connection to the service is lost.
    // WFS_ERR_INTERNAL_ERROR   An internal inconsistency or other unexpected error occurred in the XFS subsystem.
    // WFS_ERR_INVALID_HSERVICE The hService parameter is not a valid service handle.
    // WFS_ERR_INVALID_REQ_ID   The ReqID parameter does not correspond to an outstanding request on the service.
    return WFS_SUCCESS;
}
HRESULT SPI_API WFPSetTraceLevel(HSERVICE hService, DWORD dwTraceLevel) {
    if (!pcsc.isValid(hService))
        return WFS_ERR_INVALID_HSERVICE;
    pcsc.get(hService).setTraceLevel(dwTraceLevel);
    // Codes de fin possibles pour la fonction :
    // WFS_ERR_CONNECTION_LOST    The connection to the service is lost.
    // WFS_ERR_INTERNAL_ERROR     An internal inconsistency or other unexpected error occurred in the XFS subsystem.
    // WFS_ERR_INVALID_HSERVICE   The hService parameter is not a valid service handle.
    // WFS_ERR_INVALID_TRACELEVEL The dwTraceLevel parameter does not correspond to a valid trace level or set of levels.
    // WFS_ERR_NOT_STARTED        The application has not previously performed a successful WFSStartUp.
    // WFS_ERR_OP_IN_PROGRESS     A blocking operation is in progress on the thread; only WFSCancelBlockingCall and WFSIsBlocking are permitted at this time.
    return WFS_SUCCESS;
}
/** Appelé par XFS pour déterminer si DLL peut être déchargée avec ce fournisseur de services directement maintenant. */
HRESULT SPI_API WFPUnloadService() {
    // Codes de fin possibles pour la fonction :
    // WFS_ERR_NOT_OK_TO_UNLOAD
    //     The XFS Manager may not unload the service provider DLL at this time. It will repeat this
    //     request to the service provider until the return is WFS_SUCCESS, or until a new session is
    //     started by an application with this service provider.

    return pcsc.isEmpty() ? WFS_SUCCESS : WFS_ERR_NOT_OK_TO_UNLOAD;
}
} // extern "C"