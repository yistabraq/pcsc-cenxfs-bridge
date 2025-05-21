#ifndef PCSC_CENXFS_BRIDGE_PCSC_Context_H
#define PCSC_CENXFS_BRIDGE_PCSC_Context_H

#pragma once

#include <boost/noncopyable.hpp>

// PC/SC API
#include <winscard.h>

namespace PCSC {
    /** Un contexte représente une connexion au sous-système PC/SC. Il est isolé dans une classe distincte
        principalement pour s'assurer que son destructeur ferme le contexte PC/SC au tout dernier moment,
        lorsque tous les autres objets dépendant du contexte auront déjà été détruits.
        Ainsi, il est destiné à être hérité par la classe manager.
    */
    class Context : private boost::noncopyable {
        /// Le contexte du sous-système PC/SC.
        SCARDCONTEXT hContext;
    public:
        /// Ouvre une connexion au sous-système PC/SC.
        Context();
        /// Ferme la connexion au sous-système PC/SC.
        ~Context();
    public:// Accès aux internes
        inline SCARDCONTEXT context() const { return hContext; }
    };
} // namespace PCSC
#endif PCSC_CENXFS_BRIDGE_PCSC_Context_H