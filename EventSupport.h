#ifndef PCSC_CENXFS_BRIDGE_EventSupport_H
#define PCSC_CENXFS_BRIDGE_EventSupport_H


#include <set>
// Для std::pair
#include <utility>
#include <cassert>
// Для HWND и DWORD
#include <windef.h>
// Для PostMessage
#include <winuser.h>

class EventSubscriber {
    /// Окно, которое должно получать указанные события.
    HWND hWnd;
    /// Битовая маска активированных для получения событий:
    ///   SERVICE_EVENTS
    ///   USER_EVENTS
    ///   SYSTEM_EVENTS
    ///   EXECUTE_EVENTS
    DWORD mask;

    friend class EventNotifier;
public:
    /*enum Event {
        /// Изменение статуса устройства (например, доступность).
        Service = SERVICE_EVENTS,
        /// Устройство требует внимания со стороны пользователя, например, замены бумаги в принтере.
        User    = USER_EVENTS,
        /// События от "железа", например, ошибки "железа", недостаочно места на диске, несоответствие версий.
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
    /// Добавляет к маске новое отслеживаемое событие.
    void add(DWORD event) {
        mask |= event;
    }
    /// Удаляет из маски указанный класс отслеживаемых событий.
    /// @return `true`, если после удаления события больше нет подписки ни на одно из событий, иначе `false`.
    bool remove(DWORD event) {
        mask &= ~event;
        return mask == 0;
    }
    void notify(DWORD event, WFSRESULT* result) const {
        if (mask & event) {
            PostMessage(hWnd, (DWORD)event, 0, (LPARAM)result);
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
        // Если указанный элемент уже существовал в карте, то он не будет заменен,
        // нам это и не нужно. Вместо этого нужно обновить существующий элемент.
        if (r.second) {
            r.first->add(event);
        }
    }
    void remove(HWND hWnd, DWORD event) {
        // NULL означает, что производится отписка для всех окон
        if (hWnd == NULL) {
            std::vector<HWND> forRemove;
            for (SubscriberList::iterator it = subscribers.begin(); it != subscribers.end(); ++it) {
                // Удаляем класс событий. Если это был последний интересуемый класс событий,
                // то удаляем и подписчика. Если указан 0, то отписка идет от всех классов событий.
                if (event == 0 || it->remove(event)) {
                    // Для C++11 можно было бы использовать it = subscribers.erase(it);
                    // Но хочется остаться в рамках C++03
                    forRemove.push_back(it->hWnd);
                }
            }
            for (std::vector<HWND>::const_iterator it = forRemove.begin(); it != forRemove.end(); ++it) {
                // Второй параметр конструктора не важен.
                subscribers.erase(EventSubscriber(*it, 0));
            }
        } else {
            SubscriberList::iterator it = subscribers.find(EventSubscriber(hWnd, event));
            if (it != subscribers.end()) {
                // Удаляем класс событий. Если это был последний интересуемый класс событий,
                // то удаляем и подписчика. Если указан 0, то отписка идет от всех классов событий.
                if (event == 0 || it->remove(event)) {
                    subscribers.erase(it);
                }
            }
        }
    }
    /// Удаляет всех подписчиков на события.
    void clear() {
        subscribers.clear();
    }
    /// Уведомляет всех подписчиков об указанном событии.
    void notify(DWORD event, WFSRESULT* result) const {
        for (SubscriberList::const_iterator it = subscribers.begin(); it != subscribers.end(); ++it) {
            //TODO: Копирование result -- чтобы у каждого подписчика был свой.
            it->notify(event, result);
        }
    }
};

#endif // PCSC_CENXFS_BRIDGE_EventSupport_H