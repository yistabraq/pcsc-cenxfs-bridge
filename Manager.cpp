#include "Manager.h"

#include "Service.h"
#include "Settings.h"

#include "XFS/Logger.h"

Manager::Manager() : readerChangesMonitor(*this) {
    //XFS::Logger() << "Manager::Manager - Manager instance created";
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Service& Manager::create(HSERVICE hService, const Settings& settings) {
    //XFS::Logger() << "Manager::create - Creating new service for hService=" << hService;
    Service& result = services.create(*this, hService, settings);
    //XFS::Logger() << "Manager::create - Service created successfully";
    
    // Interrompt l'attente du thread sur SCardGetStatusChange, car il est nécessaire de livrer
    // au nouveau service des informations sur tous les lecteurs actuellement existants.
    //XFS::Logger() << "Manager::create - Cancelling reader changes monitor to update new service";
    readerChangesMonitor.cancel("Manager::create");
    //XFS::Logger() << "Manager::create - Reader changes monitor cancelled";
    return result;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void Manager::notifyChanges(const SCARD_READERSTATE& state, bool deviceChange) {
    //XFS::Logger() << "Manager::notifyChanges - Notifying changes for reader [" << state.szReader << "], deviceChange=" << deviceChange;
    // Notifie d'abord les auditeurs abonnés des changements, et seulement ensuite
    // essaie de terminer les tâches.
    services.notifyChanges(state, deviceChange);
    //XFS::Logger() << "Manager::notifyChanges - Services notified";
    tasks.notifyChanges(state, deviceChange);
    //XFS::Logger() << "Manager::notifyChanges - Tasks notified";
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void Manager::addTask(const Task::Ptr& task) {
    //XFS::Logger() << "Manager::addTask - Adding new task";
    if (tasks.addTask(task)) {
        //XFS::Logger() << "Manager::addTask - Task added successfully, cancelling reader changes monitor";
        // Interrompt l'attente du thread sur SCardGetStatusChange, car il faut maintenant attendre
        // jusqu'à un nouveau délai d'attente. L'attente avec le nouveau délai démarrera automatiquement.
        readerChangesMonitor.cancel("Manager::addTask");
        //XFS::Logger() << "Manager::addTask - Reader changes monitor cancelled";
    } else {
        //XFS::Logger() << "Manager::addTask - Task not added (no timeout change)";
    }
}
bool Manager::cancelTask(HSERVICE hService, REQUESTID ReqID) {
    //XFS::Logger() << "Manager::cancelTask - Cancelling task for hService=" << hService << ", ReqID=" << ReqID;
    if (tasks.cancelTask(hService, ReqID)) {
        //XFS::Logger() << "Manager::cancelTask - Task cancelled successfully, cancelling reader changes monitor";
        // Interrompt l'attente du thread sur SCardGetStatusChange, car il faut maintenant attendre
        // jusqu'à un nouveau délai d'attente. L'attente avec le nouveau délai démarrera automatiquement.
        readerChangesMonitor.cancel("Manager::cancelTask");
        //XFS::Logger() << "Manager::cancelTask - Reader changes monitor cancelled";
        return true;
    }
    //XFS::Logger() << "Manager::cancelTask - Task not found or already cancelled";
    return false;
}