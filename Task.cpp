#include "Task.h"

#include "Service.h"

#include "XFS/Result.h"

#include <cassert>
#include <sstream>

void Task::complete(HRESULT result) const {
    //XFS::Logger() << "Task::complete - Completing task with result: " << result;
    XFS::Result(ReqID, serviceHandle(), result).attach((WFSIDCCARDDATA**)0).send(hWnd, WFS_EXECUTE_COMPLETE);
    //XFS::Logger() << "Task::complete - Task completed successfully";
}

HSERVICE Task::serviceHandle() const {
    return mService.handle();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
TaskContainer::~TaskContainer() {
    //XFS::Logger() << "TaskContainer::~TaskContainer - Destroying task container";
    boost::lock_guard<boost::recursive_mutex> lock(tasksMutex);
    for (TaskList::iterator it = tasks.begin(); it != tasks.end(); ++it) {
        (*it)->cancel();
    }
    tasks.clear();
    //XFS::Logger() << "TaskContainer::~TaskContainer - All tasks cancelled and container cleared";
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool TaskContainer::addTask(const Task::Ptr& task) {
    //XFS::Logger() << "TaskContainer::addTask - Adding new task";
    boost::lock_guard<boost::recursive_mutex> lock(tasksMutex);

    std::pair<TaskList::iterator, bool> r = tasks.insert(task);
    // Les éléments insérés doivent être uniques par ReqID.
    assert(r.second == true && "[TaskContainer::addTask] Ajout d'une tâche avec un <ServiceHandle, ReqID> existant");
    assert(!tasks.empty() &&  "[TaskContainer::addTask] L'insertion de la tâche n'a pas été effectuée");
    
    // Obtient le premier index -- par temps de deadline
    typedef TaskList::nth_index<0>::type Index0;

    Index0& byDeadline = tasks.get<0>();
    bool isFirst = *byDeadline.begin() == task;
    //XFS::Logger() << "TaskContainer::addTask - Task added, is first in queue: " << isFirst;
    return isFirst;
}

bool TaskContainer::cancelTask(HSERVICE hService, REQUESTID ReqID) {
    //XFS::Logger() << "TaskContainer::cancelTask - Cancelling task for hService=" << hService << ", ReqID=" << ReqID;
    boost::lock_guard<boost::recursive_mutex> lock(tasksMutex);

    // Obtient le deuxième index -- par numéro de suivi
    typedef TaskList::nth_index<1>::type Index1;

    Index1& byID = tasks.get<1>();
    Index1::iterator it = byID.find(boost::make_tuple(hService, ReqID));
    if (it == byID.end()) {
        //XFS::Logger() << "TaskContainer::cancelTask - Task not found";
        return false;
    }
    // Signale aux auditeurs enregistrés que la tâche est annulée.
    (*it)->cancel();
    byID.erase(it);
    //XFS::Logger() << "TaskContainer::cancelTask - Task cancelled and removed successfully";
    return true;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
DWORD TaskContainer::getTimeout() const {
    //XFS::Logger() << "TaskContainer::getTimeout - Calculating timeout";
    boost::lock_guard<boost::recursive_mutex> lock(tasksMutex);

    // S'il y a des tâches, on attend jusqu'à leur timeout, sinon jusqu'à l'infini.
    if (!tasks.empty()) {
        // Temps jusqu'au prochain timeout.
        bc::steady_clock::duration dur = (*tasks.begin())->deadline - bc::steady_clock::now();
        // Convertit en millisecondes
        DWORD timeout = (DWORD)bc::duration_cast<bc::milliseconds>(dur).count();
        //XFS::Logger() << "TaskContainer::getTimeout - Next timeout in " << timeout << "ms";
        return timeout;
    }
    //XFS::Logger() << "TaskContainer::getTimeout - No tasks, returning INFINITE";
    return INFINITE;
}

void TaskContainer::processTimeouts(bc::steady_clock::time_point now) {
    //XFS::Logger() << "TaskContainer::processTimeouts - Processing timeouts at " << now;
    boost::lock_guard<boost::recursive_mutex> lock(tasksMutex);

    // Obtient le premier index -- par temps de deadline
    typedef TaskList::nth_index<0>::type Index0;

    Index0& byDeadline = tasks.get<0>();
    Index0::iterator begin = byDeadline.begin();
    Index0::iterator it = begin;
    for (; it != byDeadline.end(); ++it) {
        // Extrait toutes les tâches dont le timeout est atteint. Comme toutes les tâches sont ordonnées par
        // temps de timeout (plus le timeout est proche, plus la tâche est proche du début de la file), alors la première
        // tâche dont le timeout n'est pas encore atteint interrompt la chaîne des timeouts.
        if ((*it)->deadline > now) {
            break;
        }
        // Signale aux auditeurs enregistrés que le timeout est survenu.
        (*it)->timeout();
    }
    // Supprime les tâches terminées par timeout.
    byDeadline.erase(begin, it);
    //XFS::Logger() << "TaskContainer::processTimeouts - Timeouts processed";
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void TaskContainer::notifyChanges(const SCARD_READERSTATE& state, bool deviceChange) {
    //XFS::Logger() << "TaskContainer::notifyChanges - Notifying tasks of reader changes, deviceChange=" << deviceChange;
    boost::lock_guard<boost::recursive_mutex> lock(tasksMutex);
    for (TaskList::iterator it = tasks.begin(); it != tasks.end();) {
        // Si la tâche attendait cet événement, alors on la supprime de la liste.
        if ((*it)->match(state, deviceChange)) {
            it = tasks.erase(it);
            //XFS::Logger() << "TaskContainer::notifyChanges - Task matched and removed";
            continue;
        }
        ++it;
    }
    //XFS::Logger() << "TaskContainer::notifyChanges - Changes notification completed";
}