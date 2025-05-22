#ifndef PCSC_CENXFS_BRIDGE_XFS_Result_H
#define PCSC_CENXFS_BRIDGE_XFS_Result_H

#pragma once

#include "Utils/CTString.h"
#include "Utils/Enum.h"

#include "PCSC/Status.h"

#include "XFS/Logger.h"
#include "XFS/Memory.h"
#include "XFS/Status.h"

#include <cassert>
#include <sstream>
#include <string>

// Pour REQUESTID, HSERVICE, HRESULT, LONG et DWORD
#include <windef.h>
// Pour PostMessage
#include <winuser.h>
// Pour GetSystemTime
#include <WinBase.h>
// Définitions pour les lecteurs de cartes (Identification card unit (IDC))
#include <XFSIDC.h>

namespace XFS {
    class MsgType : public Enum<DWORD, MsgType> {
        typedef Enum<DWORD, MsgType> _Base;
    public:
        MsgType(DWORD value) : _Base(value) {}

        CTString name() const {
            static CTString names1[] = {
                CTString("WFS_OPEN_COMPLETE"      ), // (WM_USER + 1)
                CTString("WFS_CLOSE_COMPLETE"     ), // (WM_USER + 2)
                CTString("WFS_LOCK_COMPLETE"      ), // (WM_USER + 3)
                CTString("WFS_UNLOCK_COMPLETE"    ), // (WM_USER + 4)
                CTString("WFS_REGISTER_COMPLETE"  ), // (WM_USER + 5)
                CTString("WFS_DEREGISTER_COMPLETE"), // (WM_USER + 6)
                CTString("WFS_GETINFO_COMPLETE"   ), // (WM_USER + 7)
                CTString("WFS_EXECUTE_COMPLETE"   ), // (WM_USER + 8)
            };
            static CTString names2[] = {
                CTString("WFS_EXECUTE_EVENT"      ), // (WM_USER + 20)
                CTString("WFS_SERVICE_EVENT"      ), // (WM_USER + 21)
                CTString("WFS_USER_EVENT"         ), // (WM_USER + 22)
                CTString("WFS_SYSTEM_EVENT"       ), // (WM_USER + 23)
            };
            static CTString names3[] = {
                CTString("WFS_TIMER_EVENT"        ), // (WM_USER + 100)
            };
            CTString result;
            if (_Base::name(names1, mValue - (WM_USER + 1), result)) {
                return result;
            }
            if (_Base::name(names2, mValue - (WM_USER + 20), result)) {
                return result;
            }
            return _Base::name(names3, mValue - (WM_USER + 100));
        }
    };

    /** Classe pour représenter le résultat de l'exécution d'une méthode XFS SPI. */
    class Result {
        WFSRESULT* pResult;
    public:
        Result(REQUESTID ReqID, HSERVICE hService, PCSC::Status result) {
            init(ReqID, hService, result.translate());
        }
        Result(REQUESTID ReqID, HSERVICE hService, HRESULT result) {
            init(ReqID, hService, result);
        }
    public:// Remplissage des résultats des commandes WFPGetInfo
        /// Attache les données de statut spécifiées au résultat.
        inline Result& attach(WFSIDCSTATUS* data) {
            assert(pResult != NULL);
            pResult->u.dwCommandCode = WFS_INF_IDC_STATUS;
            pResult->lpBuffer = data;
            return *this;
        }
        /// Attache les données de capacités de l'appareil spécifiées au résultat.
        inline Result& attach(WFSIDCCAPS* data) {
            assert(pResult != NULL);
            pResult->u.dwCommandCode = WFS_INF_IDC_CAPABILITIES;
            pResult->lpBuffer = data;
            return *this;
        }
    public:// Remplissage des résultats des commandes WFPExecute
        /// Attache les données de lecture de carte spécifiées au résultat.
        inline Result& attach(WFSIDCCARDDATA** data) {
            assert(pResult != NULL);
            assert(pResult->lpBuffer == NULL && "Result already has data!");
            pResult->u.dwCommandCode = WFS_CMD_IDC_READ_RAW_DATA;
            pResult->lpBuffer = data;
            return *this;
        }
        /// Attache les données de réponse de puce spécifiées au résultat.
        inline Result& attach(WFSIDCCHIPIO* data) {
            assert(pResult != NULL);
            assert(pResult->lpBuffer == NULL && "Result already has data!");
            pResult->u.dwCommandCode = WFS_CMD_IDC_CHIP_IO;
            pResult->lpBuffer = data;
            return *this;
        }
        inline Result& attach(WFSIDCCHIPPOWEROUT* data) {
            assert(pResult != NULL);
            assert(pResult->lpBuffer == NULL && "Result already has data!");
            pResult->u.dwCommandCode = WFS_CMD_IDC_CHIP_POWER;
            pResult->lpBuffer = data;
            return *this;
        }
    public:
        /// Attache les données de capacités de l'appareil spécifiées au résultat.
        inline Result& attach(WFSDEVSTATUS* data) {
            assert(pResult != NULL);
            assert(pResult->lpBuffer == NULL && "Result already has data!");
            pResult->u.dwEventID = WFS_SYSE_DEVICE_STATUS;
            pResult->lpBuffer = data;
            return *this;
        }
    public:// Événements de disponibilité de la carte dans le lecteur.
        /// L'événement est généré lorsque le lecteur détecte qu'une carte y a été insérée.
        inline Result& cardInserted() {
            assert(pResult != NULL);
            assert(pResult->lpBuffer == NULL && "Result already has data!");
            pResult->u.dwEventID = WFS_EXEE_IDC_MEDIAINSERTED;
            return *this;
        }
        /// L'événement est généré lorsque le lecteur détecte qu'une carte en a été retirée.
        inline Result& cardRemoved() {
            assert(pResult != NULL);
            assert(pResult->lpBuffer == NULL && "Result already has data!");
            pResult->u.dwEventID = WFS_SRVE_IDC_MEDIAREMOVED;
            return *this;
        }
        /// Généré si une carte a été détectée dans le lecteur pendant la commande
        /// de réinitialisation (WFS_CMD_IDC_RESET). Comme la carte est détectée, elle est considérée comme
        /// prête à être lue.
        /// TODO: Cela pourrait ne pas être le cas ?
        inline Result& cardDetected() {
            assert(pResult != NULL);
            assert(pResult->lpBuffer == NULL && "Result already has data!");
            pResult->u.dwEventID = WFS_SRVE_IDC_MEDIADETECTED;
            //TODO: Il pourrait être nécessaire d'allouer de la mémoire via WFSAllocateMore
            pResult->lpBuffer = XFS::alloc<DWORD>(WFS_IDC_CARDREADPOSITION);
            return *this;
        }
    public:
        inline Result& eject() {
            assert(pResult != NULL);
            assert(pResult->lpBuffer == NULL && "Result already has data!");
            pResult->u.dwEventID = WFS_CMD_IDC_EJECT_CARD;
            return *this;
        }
        void send(HWND hWnd, DWORD messageType) {
            assert(pResult != NULL);
            Logger() << "Result::send(hWnd=" << hWnd << ", type=" << MsgType(messageType)
                     << ") with result " << Status(pResult->hResult) << " for ReqID=" << pResult->RequestID;
            PostMessage(hWnd, messageType, NULL, (LPARAM)pResult);
        }
    private:
        inline void init(REQUESTID ReqID, HSERVICE hService, HRESULT result) {
            pResult = XFS::alloc<WFSRESULT>();
            pResult->RequestID = ReqID;
            pResult->hService = hService;
            pResult->hResult = result;
            GetSystemTime(&pResult->tsTimestamp);

            assert(pResult->lpBuffer == NULL);
        }
    };
} // namespace XFS
#endif // PCSC_CENXFS_BRIDGE_XFS_Result_H