#ifndef PCSC_CENXFS_BRIDGE_Settings_H
#define PCSC_CENXFS_BRIDGE_Settings_H

#pragma once

#include <string>

class Settings
{
public:
    /// Contient les paramètres pour contourner divers problèmes.
    class Workarounds {
    public:
        /// Contient les paramètres pour émuler la deuxième piste.
        class Track2 {
        public:
            /** Informer le gestionnaire XFS de la possibilité de lire la piste 2. Kalignite plante si le lecteur
                ne peut pas lire la deuxième piste magnétique, donc ce paramètre est obligatoire pour lui.
            @par Effet
                Si activé, lors de l'interrogation des capacités de l'appareil, il est indiqué que l'appareil peut lire
                la deuxième piste. De plus, le paramètre `value` entre en vigueur, contenant la valeur
                de la deuxième piste telle qu'elle sera communiquée au gestionnaire XFS.
            @par Valeur par défaut
                Par défaut, le paramètre est désactivé, c'est-à-dire que le gestionnaire XFS est informé que la lecture de la
                deuxième piste n'est pas prise en charge.
            */
            bool report;
            /** Valeur de la deuxième piste, telle qu'elle sera communiquée au gestionnaire XFS. Cela signifie qu'elle
                ne doit pas contenir les séparateurs de début et de fin. Cette valeur n'est utilisée
                que si `report` est `true`.
            @par Valeur par défaut
                Par défaut, contient une chaîne vide, ce qui signifie que lors de l'interrogation des données de la piste, la lecture
                se terminera avec le statut `WFS_IDC_DATAMISSING`.
            */
            std::string value;
        public:
            Track2() : report(false) {}
        };
    public:
        /** Kalignite, pour une raison quelconque, lorsqu'il utilise le protocole de communication T0, envoie des commandes pour
            transmettre des données à la puce via le protocole T1, en ajoutant à la fin un octet avec le nombre d'octets attendus en
            réponse (dans les expériences, il y avait toujours 0, c'est-à-dire 256 octets attendus). Cependant, la transmission
            d'un tel tampon à SCardTransmit fait qu'elle renvoie le code d'erreur 0x57, ce qui
            signifie des données incorrectes dans le tampon.
        @par Effet
            Si ce paramètre est activé, toutes les données transmises via le protocole T0 sont analysées
            et si nécessaire, la longueur des données est corrigée de manière à tronquer les octets
            superflus.
        @par Valeur par défaut
            Par défaut, le paramètre est désactivé, c'est-à-dire que l'analyse n'est pas effectuée.
        */
        bool correctChipIO;
        /** La fonctionnalité d'éjection de carte est déclarée obligatoire à implémenter, mais il est prévu que
            elle peut être absente du lecteur. Dans ce cas, la commande d'éjection de carte doit
            renvoyer un code de réponse indiquant que cette commande n'est pas prise en charge. Kalignite, cependant,
            ne s'attend pas à un tel code de réponse, bien que dans les capacités de l'appareil il soit explicitement indiqué que l'éjection
            de carte n'est pas prise en charge.
        @par Effet
            Si ce paramètre est activé, la commande d'éjection de carte est toujours réussie.
        @par Valeur par défaut
            Par défaut, le paramètre est désactivé, c'est-à-dire qu'en réponse à la commande d'éjection, le code de réponse "Non pris en charge" est donné.
        */
        bool canEject;
        /// Kalignite exige que le lecteur puisse lire les pistes 2, donc nous avons
        /// ce paramètre.
        Track2 track2;
    public:
        Workarounds() : correctChipIO(false) {}
    };
public:// Paramètres non relus.
    /// Nom du fournisseur lui-même. Ne change pas après la création des paramètres.
    std::string providerName;
public:// Paramètres relus par la fonction reread()
    /// Nom du lecteur avec lequel le fournisseur doit fonctionner.
    std::string readerName;
    /// Niveau de détail des messages affichés, plus il est élevé, plus les messages sont détaillés.
    /// Niveau 0 -- les messages ne sont pas affichés.
    int traceLevel;
    /** Si `true`, lors de l'ouverture de la connexion avec la carte, elle est ouverte en mode exclusif.
    @par Valeur par défaut
        Par défaut, le mode exclusif n'est pas utilisé.
    */
    bool exclusive;
    /// Paramètres concernant le contournement des bugs d'implémentation du sous-système XFS dans Kalignite.
    Workarounds workarounds;
public:
    Settings(const char* serviceName, int traceLevel);

    /// Relit tous les paramètres du fournisseur de service, sauf le nom du fournisseur de service.
    void reread();
    std::string toJSONString() const;
};

#endif // PCSC_CENXFS_BRIDGE_Settings_H