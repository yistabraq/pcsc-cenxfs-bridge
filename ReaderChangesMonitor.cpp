#include "ReaderChangesMonitor.h"

#include "Manager.h"

#include "PCSC/ReaderState.h"
#include "PCSC/Status.h"

#include "XFS/Logger.h"

ReaderChangesMonitor::ReaderChangesMonitor(Manager& manager)
    : manager(manager), stopRequested(false)
{
    //XFS::Logger() << "ReaderChangesMonitor::ReaderChangesMonitor - Starting monitor thread";
    // Démarre le thread d'attente des changements.
    waitChangesThread.reset(new boost::thread(&ReaderChangesMonitor::run, this));
    //XFS::Logger() << "ReaderChangesMonitor::ReaderChangesMonitor - Monitor thread started successfully";
}
ReaderChangesMonitor::~ReaderChangesMonitor() {
    //XFS::Logger() << "ReaderChangesMonitor::~ReaderChangesMonitor - Stopping monitor thread";
    // Demande l'arrêt du thread.
    stopRequested = true;
    // Signale qu'il est nécessaire d'interrompre l'attente
    cancel("ReaderChangesMonitor::~ReaderChangesMonitor");
    // Attend qu'il se termine.
    waitChangesThread->join();
    //XFS::Logger() << "ReaderChangesMonitor::~ReaderChangesMonitor - Monitor thread stopped successfully";
}
void ReaderChangesMonitor::run() {
    //XFS::Logger() << "ReaderChangesMonitor::run - Starting reader changes dispatch thread";
    // L'état est initialement inconnu de nous.
    DWORD readersState = SCARD_STATE_UNAWARE;
    while (!stopRequested) {
        // En entrée, l'état actuel des lecteurs -- en sortie, le nouvel état.
        readersState = getReadersAndWaitChanges(readersState);
    }
    //XFS::Logger() << "ReaderChangesMonitor::run - Reader changes dispatch thread stopped";
}
/// Obtient la liste des noms de lecteurs à partir d'une chaîne contenant tous les noms, séparés par le caractère '\0'.
std::vector<const char*> getReaderNames(const std::vector<char>& readerNames) {
    XFS::Logger l;
    l << "Avalible readers:";
    std::size_t i = 0;
    std::size_t k = 0;
    std::vector<const char*> names;
    const std::size_t size = readerNames.size();
    while (i < size) {
        const char* name = &readerNames[i];
        // Cherche le début de la ligne suivante.
        while (i < size && readerNames[i] != '\0') ++i;
        // Saute le '\0'.
        ++i;

        if (i < size) {
            l << '\n' << ++k << ": " << name;
            names.push_back(name);
        }
    }
    return names;
}
DWORD ReaderChangesMonitor::getReadersAndWaitChanges(DWORD readersState) {
    //XFS::Logger() << "ReaderChangesMonitor::getReadersAndWaitChanges - Getting readers list";
    DWORD readersCount = 0;
    // Détermine les lecteurs disponibles : d'abord la quantité, puis les lecteurs eux-mêmes.
    PCSC::Status st = SCardListReaders(manager.context(), NULL, NULL, &readersCount);
    //XFS::Logger() << "ReaderChangesMonitor::getReadersAndWaitChanges - SCardListReaders[count] result: " << st;

    // Obtient les noms des lecteurs disponibles.
    std::vector<char> readerNames(readersCount);
    if (readersCount != 0) {
        st = SCardListReaders(manager.context(), NULL, &readerNames[0], &readersCount);
        //XFS::Logger() << "ReaderChangesMonitor::getReadersAndWaitChanges - SCardListReaders[data] result: " << st;
    }

    std::vector<const char*> names = getReaderNames(readerNames);
    //XFS::Logger() << "ReaderChangesMonitor::getReadersAndWaitChanges - Found " << names.size() << " readers";

    // Se prépare à attendre les événements de tous les lecteurs détectés et
    // à l'évolution de leur nombre.
    std::vector<SCARD_READERSTATE> readers(1 + names.size());
    // Lecteur avec un nom spécial, signifiant qu'il est nécessaire de surveiller
    // l'apparition/disparition des lecteurs.
    readers[0].szReader = "\\\\?PnP?\\Notification";
    readers[0].dwCurrentState = readersState;

    // Remplit les structures pour attendre les événements des lecteurs trouvés.
    for (std::size_t i = 0; i < names.size(); ++i) {
        readers[1 + i].szReader = names[i];
    }

    // Attend les événements des lecteurs.
    while (!waitChanges(readers)) {
        if (stopRequested) {
            //XFS::Logger() << "ReaderChangesMonitor::getReadersAndWaitChanges - Stop requested, exiting";
            return 0;
        }
    }
    //XFS::Logger() << "ReaderChangesMonitor::getReadersAndWaitChanges - Reader changes detected";
    return readers[0].dwCurrentState;
}
bool ReaderChangesMonitor::waitChanges(std::vector<SCARD_READERSTATE>& readers) {
    //XFS::Logger() << "ReaderChangesMonitor::waitChanges - Waiting for reader changes";
    PCSC::Status st = SCardGetStatusChange(manager.context(), manager.getTimeout(), &readers[0], (DWORD)readers.size());
    //XFS::Logger() << "ReaderChangesMonitor::waitChanges - SCardGetStatusChange result: " << st;

    if (st.value() == SCARD_E_TIMEOUT) {
        //XFS::Logger() << "ReaderChangesMonitor::waitChanges - Processing timeouts";
        manager.processTimeouts(bc::steady_clock::now());
    }

    bool readersChanged = false;
    bool first = true;
    for (std::vector<SCARD_READERSTATE>::iterator it = readers.begin(); it != readers.end(); ++it) {
        PCSC::ReaderState diff = PCSC::ReaderState(it->dwCurrentState ^ it->dwEventState);
        //XFS::Logger() << "ReaderChangesMonitor::waitChanges - Reader [" << it->szReader << "] state change:";
        //XFS::Logger() << "  Old state: " << PCSC::ReaderState(it->dwCurrentState);
        //XFS::Logger() << "  New state: " << PCSC::ReaderState(it->dwEventState);
        //XFS::Logger() << "  Diff: " << diff;

        if (it->dwEventState & SCARD_STATE_CHANGED) {
            if (first) {
                readersChanged = true;
                //XFS::Logger() << "ReaderChangesMonitor::waitChanges - Device change detected";
            }
            //XFS::Logger() << "ReaderChangesMonitor::waitChanges - Notifying changes for reader [" << it->szReader << "]";
            manager.notifyChanges(*it, first);
        }
        it->dwCurrentState = it->dwEventState;
        first = false;
    }
    //XFS::Logger() << "ReaderChangesMonitor::waitChanges - Changes processed, readersChanged=" << readersChanged;
    return readersChanged;
}
void ReaderChangesMonitor::cancel(const char* reason) const {
    //XFS::Logger() << "ReaderChangesMonitor::cancel - Cancelling wait operation, reason: " << reason;
    PCSC::Status st = SCardCancel(manager.context());
    //XFS::Logger() << "ReaderChangesMonitor::cancel - SCardCancel result: " << st;
}