#include "Manager.h"
#include "Service.h"
#include "Settings.h"

#include "PCSC/Status.h"

#include "XFS/Result.h"
#include "XFS/Logger.h"  // Pour le logging

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
    XFS::Logger() << "WFPOpen called with logical name: " << lpszLogicalName << ", timeout: " << dwTimeOut;
    
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
    
    XFS::Logger() << "WFPOpen completed successfully";
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
    XFS::Logger() << "WFPClose called for service: " << hService;
    
    if (!pcsc.isValid(hService)) {
        XFS::Logger() << "WFPClose failed: Invalid service handle";
        return WFS_ERR_INVALID_HSERVICE;
    }
    
    pcsc.remove(hService);
    XFS::Result(ReqID, hService, WFS_SUCCESS).send(hWnd, WFS_CLOSE_COMPLETE);
    
    XFS::Logger() << "WFPClose completed successfully";
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
    XFS::Logger() << "WFPRegister called for service: " << hService << ", event class: 0x" << std::hex << dwEventClass << std::dec;
    
    if (!pcsc.addSubscriber(hService, hWndReg, dwEventClass)) {
        XFS::Logger() << "WFPRegister failed: Invalid service handle";
        return WFS_ERR_INVALID_HSERVICE;
    }
    
    XFS::Result(ReqID, hService, WFS_SUCCESS).send(hWnd, WFS_REGISTER_COMPLETE);
    XFS::Logger() << "WFPRegister completed successfully";
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
    XFS::Logger() << "WFPDeregister called for service: " << hService << ", event class: 0x" << std::hex << dwEventClass << std::dec;

    // Se désabonne des événements. Si personne n'a été supprimé, personne n'était enregistré.
    if (!pcsc.removeSubscriber(hService, hWndReg, dwEventClass)) {
        XFS::Logger() << "WFPDeregister failed: Invalid service handle";
        return WFS_ERR_INVALID_HSERVICE;
    }
    // La suppression de l'abonné est toujours réussie.
    XFS::Result(ReqID, hService, WFS_SUCCESS).send(hWnd, WFS_DEREGISTER_COMPLETE);
    XFS::Logger() << "WFPDeregister completed successfully";
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
    XFS::Logger() << "WFPLock called for service: " << hService << ", timeout: " << dwTimeOut;
    
    if (!pcsc.isValid(hService)) {
        XFS::Logger() << "WFPLock failed: Invalid service handle";
        return WFS_ERR_INVALID_HSERVICE;
    }

    PCSC::Status st = pcsc.get(hService).lock();
    XFS::Result(ReqID, hService, st).send(hWnd, WFS_LOCK_COMPLETE);
    
    XFS::Logger() << "WFPLock completed with status: " << st;
    return WFS_SUCCESS;
}
/**
@message WFS_UNLOCK_COMPLETE

@param hService Service auquel il faut supprimer l'accès exclusif.
@param hWnd Fenêtre qui doit recevoir le message de fin de l'opération asynchrone.
@param ReqID Identifiant de la requête qui doit être passé à la fenêtre `hWnd` à la fin de l'opération.
*/
HRESULT SPI_API WFPUnlock(HSERVICE hService, HWND hWnd, REQUESTID ReqID) {
    XFS::Logger() << "WFPUnlock called for service: " << hService;

    if (!pcsc.isValid(hService)) {
        XFS::Logger() << "WFPUnlock failed: Invalid service handle";
        return WFS_ERR_INVALID_HSERVICE;
    }

    PCSC::Status st = pcsc.get(hService).unlock();
    XFS::Result(ReqID, hService, st).send(hWnd, WFS_UNLOCK_COMPLETE);
    XFS::Logger() << "WFPUnlock completed with status: " << st;

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
    XFS::Logger() << "WFPGetInfo called for service: " << hService << ", category: 0x" << std::hex << dwCategory << std::dec << ", timeout: " << dwTimeOut;

    if (!pcsc.isValid(hService)) {
        XFS::Logger() << "WFPGetInfo failed: Invalid service handle";
        return WFS_ERR_INVALID_HSERVICE;
    }
    // Pour IDC, seules ces constantes peuvent être demandées (WFS_INF_IDC_*)
    switch (dwCategory) {
        case WFS_INF_IDC_STATUS: {      // Pas de paramètres supplémentaires
            XFS::Logger() << "WFPGetInfo: STATUS category";
            std::pair<WFSIDCSTATUS*, PCSC::Status> status = pcsc.get(hService).getStatus();
            // Obtention d'informations sur le lecteur est toujours réussie.
            XFS::Result(ReqID, hService, WFS_SUCCESS).attach(status.first).send(hWnd, WFS_GETINFO_COMPLETE);
            XFS::Logger() << "WFPGetInfo: STATUS completed";
            break;
        }
        case WFS_INF_IDC_CAPABILITIES: {// Pas de paramètres supplémentaires
            XFS::Logger() << "WFPGetInfo: CAPABILITIES category";
            std::pair<WFSIDCCAPS*, PCSC::Status> caps = pcsc.get(hService).getCaps();
            XFS::Result(ReqID, hService, caps.second).attach(caps.first).send(hWnd, WFS_GETINFO_COMPLETE);
            XFS::Logger() << "WFPGetInfo: CAPABILITIES completed with status: " << caps.second;
            break;
        }
        case WFS_INF_IDC_FORM_LIST:
        case WFS_INF_IDC_QUERY_FORM: {
            // Les formes ne sont pas supportées. La forme détermine où les données se trouvent sur les pistes.
            // Comme nous ne lisons ni n'écrivons pas les pistes, les formes ne sont pas supportées.
            XFS::Logger() << "WFPGetInfo: FORM_LIST or QUERY_FORM category (unsupported)";
            return WFS_ERR_UNSUPP_COMMAND;
        }
        default:
            XFS::Logger() << "WFPGetInfo failed: Invalid category 0x" << std::hex << dwCategory << std::dec;
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

    XFS::Logger() << "WFPGetInfo completed";
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
    XFS::Logger() << "WFPExecute called for service: " << hService << ", command: 0x" << std::hex << dwCommand << std::dec << ", timeout: " << dwTimeOut;
    
    if (!pcsc.isValid(hService)) {
        XFS::Logger() << "WFPExecute failed: Invalid service handle";
        return WFS_ERR_INVALID_HSERVICE;
    }

    switch (dwCommand) {
        // Attente d'insertion d'une carte avec délai spécifié, lecture immédiate des pistes selon la forme,
        // transmise en paramètre. Comme nous ne savons pas lire les pistes, cette commande n'est pas supportée.
        case WFS_CMD_IDC_READ_TRACK: {
            XFS::Logger() << "WFPExecute: READ_TRACK command";
            if (lpCmdData == NULL) {
                XFS::Logger() << "WFPExecute: READ_TRACK failed - NULL command data";
                return WFS_ERR_INVALID_POINTER;
            }
            LPSTR formName = (LPSTR)lpCmdData;
            XFS::Logger() << "WFPExecute: READ_TRACK form name: " << formName << " (unsupported)";
            return WFS_ERR_UNSUPP_COMMAND;
        }
        // Identique à la lecture des pistes.
        case WFS_CMD_IDC_WRITE_TRACK: {
            XFS::Logger() << "WFPExecute: WRITE_TRACK command (unsupported)";
            //WFSIDCWRITETRACK* input = (WFSIDCWRITETRACK*)lpCmdData;
            return WFS_ERR_UNSUPP_COMMAND;
        }
        // La commande demande au lecteur de retourner la carte. Comme notre lecteur ne peut rien faire
        // avec la carte, nous ne la supportons pas.
        case WFS_CMD_IDC_EJECT_CARD: {// Pas de paramètres d'entrée.
            XFS::Logger() << "WFPExecute: EJECT_CARD command";
            // Kalignite essaie d'extraire la carte, ignorant le fait que nous lui disons que cette fonctionnalité
            // n'est pas supportée, et tombe en erreur fatale si nous lui disons qu'il demande quelque chose d'impossible,
            // bien que, selon la spécification, nous devons lui dire que cette fonctionnalité n'est pas supportée
            // par le code de réponse WFS_ERR_UNSUPP_COMMAND et nous avons le droit de ne pas la supporter.
            if (pcsc.get(hService).settings().workarounds.canEject) {
                XFS::Result(ReqID, hService, WFS_SUCCESS).eject().send(hWnd, WFS_EXECUTE_COMPLETE);
                XFS::Logger() << "WFPExecute: EJECT_CARD completed (with workaround)";
                return WFS_SUCCESS;
            }
            XFS::Logger() << "WFPExecute: EJECT_CARD command (unsupported)";
            return WFS_ERR_UNSUPP_COMMAND;
        }
        // La commande capture la carte lue par le lecteur. Identique à la commande précédente.
        case WFS_CMD_IDC_RETAIN_CARD: {// Pas de paramètres d'entrée.
            XFS::Logger() << "WFPExecute: RETAIN_CARD command (unsupported)";
            return WFS_ERR_UNSUPP_COMMAND;
        }
        // La commande réinitialise le compteur de cartes capturées. Comme nous ne les capturons pas, nous ne les supportons pas.
        case WFS_CMD_IDC_RESET_COUNT: {// Pas de paramètres d'entrée.
            XFS::Logger() << "WFPExecute: RESET_COUNT command (unsupported)";
            return WFS_ERR_UNSUPP_COMMAND;
        }
        // Définit la clé DES nécessaire pour le fonctionnement du module CIM86. Nous ne la supportons pas,
        // car nous n'avons pas de tel module.
        case WFS_CMD_IDC_SETKEY: {
            XFS::Logger() << "WFPExecute: SETKEY command (unsupported)";
            //WFSIDCSETKEY* = (WFSIDCSETKEY*)lpCmdData;
            return WFS_ERR_UNSUPP_COMMAND;
        }
        // Connecte le chip, le réinitialise et lit ATR (Answer To Reset).
        // Cette commande peut également être utilisée pour un "reset froid" du chip.
        // Cette commande ne doit pas être utilisée pour un chip permanentement connecté.
        //TODO: Le chip est permanent ou non?
        case WFS_CMD_IDC_READ_RAW_DATA: {
            XFS::Logger() << "WFPExecute: READ_RAW_DATA command";
            if (lpCmdData == NULL) {
                XFS::Logger() << "WFPExecute: READ_RAW_DATA failed - NULL command data";
                return WFS_ERR_INVALID_POINTER;
            }
            // Masque binaire avec les données qui doivent être lues.
            XFS::ReadFlags readData = *((WORD*)lpCmdData);
            XFS::Logger() << "WFPExecute: READ_RAW_DATA flags: " << readData.value();
            if (readData.value() & WFS_IDC_CHIP) {
                pcsc.get(hService).asyncRead(dwTimeOut, hWnd, ReqID, readData);
                XFS::Logger() << "WFPExecute: READ_RAW_DATA async read started";
                return WFS_SUCCESS;
            }
            XFS::Logger() << "WFPExecute: READ_RAW_DATA command (unsupported flags)";
            return WFS_ERR_UNSUPP_COMMAND;
        }
        // Attend le temps spécifié jusqu'à ce qu'une carte soit insérée, puis écrit les données sur la piste spécifiée.
        // Nous ne la supportons pas, car nous ne savons pas écrire des pistes.
        case WFS_CMD_IDC_WRITE_RAW_DATA: {
            XFS::Logger() << "WFPExecute: WRITE_RAW_DATA command (unsupported)";
            //NULL-terminated array
            //WFSIDCCARDDATA** data = (WFSIDCCARDDATA**)lpCmdData;
            return WFS_ERR_UNSUPP_COMMAND;
        }
        // Envoie des données au chip et reçoit la réponse de celui-ci. Les données sont transparentes pour le fournisseur.
        case WFS_CMD_IDC_CHIP_IO: {
            XFS::Logger() << "WFPExecute: CHIP_IO command";
            if (lpCmdData == NULL) {
                XFS::Logger() << "WFPExecute: CHIP_IO failed - NULL command data";
                return WFS_ERR_INVALID_POINTER;
            }
            const WFSIDCCHIPIO* data = (const WFSIDCCHIPIO*)lpCmdData;
            std::pair<WFSIDCCHIPIO*, PCSC::Status> result = pcsc.get(hService).transmit(data);
            XFS::Logger() << "WFPExecute: CHIP_IO completed with status: " << result.second;
            XFS::Result(ReqID, hService, result.second).attach(result.first).send(hWnd, WFS_EXECUTE_COMPLETE);
            return WFS_SUCCESS;
        }
        // Déconnecte le chip.
        case WFS_CMD_IDC_RESET: {
            XFS::Logger() << "WFPExecute: RESET command";
            // Masque binaire. NULL signifie que le fournisseur décide ce que faire.
            WORD wResetIn = lpCmdData ? *((WORD*)lpCmdData) : WFS_IDC_NOACTION;
            XFS::Logger() << "WFPExecute: RESET flags: " << wResetIn;
            //TODO: Implémenter la commande WFS_CMD_IDC_RESET
            XFS::Logger() << "WFPExecute: RESET command (unsupported)";
            return WFS_ERR_UNSUPP_COMMAND;
        }
        case WFS_CMD_IDC_CHIP_POWER: {
            XFS::Logger() << "WFPExecute: CHIP_POWER command";
            if (lpCmdData == NULL) {
                XFS::Logger() << "WFPExecute: CHIP_POWER failed - NULL command data";
                return WFS_ERR_INVALID_POINTER;
            }
            WORD wChipPower = *((WORD*)lpCmdData);
            XFS::Logger() << "WFPExecute: CHIP_POWER state: " << wChipPower;
            std::pair<WFSIDCCHIPPOWEROUT*, PCSC::Status> result = pcsc.get(hService).reset(wChipPower);
            XFS::Logger() << "WFPExecute: CHIP_POWER completed with status: " << result.second;
            XFS::Result(ReqID, hService, result.second).attach(result.first).send(hWnd, WFS_EXECUTE_COMPLETE);
            return WFS_SUCCESS;
        }
        // Analyse le résultat, précédemment retourné par la commande WFS_CMD_IDC_READ_RAW_DATA. Comme nous ne la supportons pas, nous ne la supportons pas.
        case WFS_CMD_IDC_PARSE_DATA: {
            XFS::Logger() << "WFPExecute: PARSE_DATA command (unsupported)";
            // WFSIDCPARSEDATA* parseData = (WFSIDCPARSEDATA*)lpCmdData;
            return WFS_ERR_UNSUPP_COMMAND;
        }
        default: {
            XFS::Logger() << "WFPExecute failed: Invalid command 0x" << std::hex << dwCommand << std::dec;
            return WFS_ERR_INVALID_COMMAND;
        }
    }// switch (dwCommand)
    
    XFS::Logger() << "WFPExecute completed";
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
    XFS::Logger() << "WFPCancelAsyncRequest called for service: " << hService << ", request: " << ReqID;
    
    if (!pcsc.isValid(hService)) {
        XFS::Logger() << "WFPCancelAsyncRequest failed: Invalid service handle";
        return WFS_ERR_INVALID_HSERVICE;
    }
    
    if (!pcsc.cancelTask(hService, ReqID)) {
        XFS::Logger() << "WFPCancelAsyncRequest failed: Invalid request ID";
        return WFS_ERR_INVALID_REQ_ID;
    }
    
    XFS::Logger() << "WFPCancelAsyncRequest completed successfully";
    return WFS_SUCCESS;
}
HRESULT SPI_API WFPSetTraceLevel(HSERVICE hService, DWORD dwTraceLevel) {
    XFS::Logger() << "WFPSetTraceLevel called for service: " << hService << ", trace level: " << dwTraceLevel;

    if (!pcsc.isValid(hService)) {
        XFS::Logger() << "WFPSetTraceLevel failed: Invalid service handle";
        return WFS_ERR_INVALID_HSERVICE;
    }
    pcsc.get(hService).setTraceLevel(dwTraceLevel);
    // Codes de fin possibles pour la fonction :
    // WFS_ERR_CONNECTION_LOST    The connection to the service is lost.
    // WFS_ERR_INTERNAL_ERROR     An internal inconsistency or other unexpected error occurred in the XFS subsystem.
    // WFS_ERR_INVALID_HSERVICE   The hService parameter is not a valid service handle.
    // WFS_ERR_INVALID_TRACELEVEL The dwTraceLevel parameter does not correspond to a valid trace level or set of levels.
    // WFS_ERR_NOT_STARTED        The application has not previously performed a successful WFSStartUp.
    // WFS_ERR_OP_IN_PROGRESS     A blocking operation is in progress on the thread; only WFSCancelBlockingCall and WFSIsBlocking are permitted at this time.
    XFS::Logger() << "WFPSetTraceLevel completed successfully";
    return WFS_SUCCESS;
}
/** Appelé par XFS pour déterminer si DLL peut être déchargée avec ce fournisseur de services directement maintenant. */
HRESULT SPI_API WFPUnloadService() {
    XFS::Logger() << "WFPUnloadService called";

    // Codes de fin possibles pour la fonction :
    // WFS_ERR_NOT_OK_TO_UNLOAD
    //     The XFS Manager may not unload the service provider DLL at this time. It will repeat this
    //     request to the service provider until the return is WFS_SUCCESS, or until a new session is
    //     started by an application with this service provider.

    bool empty = pcsc.isEmpty();
    XFS::Logger() << "WFPUnloadService completed, can unload: " << empty;
    return empty ? WFS_SUCCESS : WFS_ERR_NOT_OK_TO_UNLOAD;
}
} // extern "C"