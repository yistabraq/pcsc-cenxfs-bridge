
// CEN/XFS API
#include <xfsspi.h>
// PC/CS API
#include <winscard.h>
// Определения для ридеров карт (Identification card unit (IDC))
#include <XFSIDC.h>

#include <sstream>
#include <iomanip>
#include <map>
#include <string>
#include <vector>
#include <cassert>
// Линкуемся с библиотекой реализации стандларта PC/SC в Windows
#pragma comment(lib, "winscard.lib")
// Линкуемся с библиотекой реализации XFS
#pragma comment(lib, "msxfs.lib")
// Линкуемся с библитекой для использования оконных функций (PostMessage)
#pragma comment(lib, "user32.lib")

#define MAKE_VERSION(major, minor) (((major) << 8) | (minor))

class CTString {
    const char* begin;
    const char* end;
public:
    CTString() : begin(NULL), end(NULL) {}
    template<std::size_t N>
    CTString(const char (&data)[N]) : begin(data), end(data + N) {}

    std::size_t size() const { return end - begin; }
    bool empty() const { return size() == 0; }
    bool isValid() const { return begin != NULL; }
};

/** Результат выполнения PC/SC функций. */
struct Status {
    LONG value;
    Status(LONG rv) : value(rv) {}

    operator bool() const { return value != 0; }

    inline const char* name() {
        static const char* names[] = {
            "SCARD_S_SUCCESS"                  , // NO_ERROR
            "SCARD_F_INTERNAL_ERROR"           , // _HRESULT_TYPEDEF_(0x80100001L)
            "SCARD_E_CANCELLED"                , // _HRESULT_TYPEDEF_(0x80100002L)
            "SCARD_E_INVALID_HANDLE"           , // _HRESULT_TYPEDEF_(0x80100003L)
            "SCARD_E_INVALID_PARAMETER"        , // _HRESULT_TYPEDEF_(0x80100004L)
            "SCARD_E_INVALID_TARGET"           , // _HRESULT_TYPEDEF_(0x80100005L)
            "SCARD_E_NO_MEMORY"                , // _HRESULT_TYPEDEF_(0x80100006L)
            "SCARD_F_WAITED_TOO_LONG"          , // _HRESULT_TYPEDEF_(0x80100007L)
            "SCARD_E_INSUFFICIENT_BUFFER"      , // _HRESULT_TYPEDEF_(0x80100008L)
            "SCARD_E_UNKNOWN_READER"           , // _HRESULT_TYPEDEF_(0x80100009L)
            "SCARD_E_TIMEOUT"                  , // _HRESULT_TYPEDEF_(0x8010000AL)
            "SCARD_E_SHARING_VIOLATION"        , // _HRESULT_TYPEDEF_(0x8010000BL)
            "SCARD_E_NO_SMARTCARD"             , // _HRESULT_TYPEDEF_(0x8010000CL)
            "SCARD_E_UNKNOWN_CARD"             , // _HRESULT_TYPEDEF_(0x8010000DL)
            "SCARD_E_CANT_DISPOSE"             , // _HRESULT_TYPEDEF_(0x8010000EL)
            "SCARD_E_PROTO_MISMATCH"           , // _HRESULT_TYPEDEF_(0x8010000FL)
            "SCARD_E_NOT_READY"                , // _HRESULT_TYPEDEF_(0x80100010L)
            "SCARD_E_INVALID_VALUE"            , // _HRESULT_TYPEDEF_(0x80100011L)
            "SCARD_E_SYSTEM_CANCELLED"         , // _HRESULT_TYPEDEF_(0x80100012L)
            "SCARD_F_COMM_ERROR"               , // _HRESULT_TYPEDEF_(0x80100013L)
            "SCARD_F_UNKNOWN_ERROR"            , // _HRESULT_TYPEDEF_(0x80100014L)
            "SCARD_E_INVALID_ATR"              , // _HRESULT_TYPEDEF_(0x80100015L)
            "SCARD_E_NOT_TRANSACTED"           , // _HRESULT_TYPEDEF_(0x80100016L)
            "SCARD_E_READER_UNAVAILABLE"       , // _HRESULT_TYPEDEF_(0x80100017L)
            "SCARD_P_SHUTDOWN"                 , // _HRESULT_TYPEDEF_(0x80100018L)
            "SCARD_E_PCI_TOO_SMALL"            , // _HRESULT_TYPEDEF_(0x80100019L)
            "SCARD_E_READER_UNSUPPORTED"       , // _HRESULT_TYPEDEF_(0x8010001AL)
            "SCARD_E_DUPLICATE_READER"         , // _HRESULT_TYPEDEF_(0x8010001BL)
            "SCARD_E_CARD_UNSUPPORTED"         , // _HRESULT_TYPEDEF_(0x8010001CL)
            "SCARD_E_NO_SERVICE"               , // _HRESULT_TYPEDEF_(0x8010001DL)
            "SCARD_E_SERVICE_STOPPED"          , // _HRESULT_TYPEDEF_(0x8010001EL)
            "SCARD_E_UNEXPECTED"               , // _HRESULT_TYPEDEF_(0x8010001FL)
            "SCARD_E_ICC_INSTALLATION"         , // _HRESULT_TYPEDEF_(0x80100020L)
            "SCARD_E_ICC_CREATEORDER"          , // _HRESULT_TYPEDEF_(0x80100021L)
            "SCARD_E_UNSUPPORTED_FEATURE"      , // _HRESULT_TYPEDEF_(0x80100022L)
            "SCARD_E_DIR_NOT_FOUND"            , // _HRESULT_TYPEDEF_(0x80100023L)
            "SCARD_E_FILE_NOT_FOUND"           , // _HRESULT_TYPEDEF_(0x80100024L)
            "SCARD_E_NO_DIR"                   , // _HRESULT_TYPEDEF_(0x80100025L)
            "SCARD_E_NO_FILE"                  , // _HRESULT_TYPEDEF_(0x80100026L)
            "SCARD_E_NO_ACCESS"                , // _HRESULT_TYPEDEF_(0x80100027L)
            "SCARD_E_WRITE_TOO_MANY"           , // _HRESULT_TYPEDEF_(0x80100028L)
            "SCARD_E_BAD_SEEK"                 , // _HRESULT_TYPEDEF_(0x80100029L)
            "SCARD_E_INVALID_CHV"              , // _HRESULT_TYPEDEF_(0x8010002AL)
            "SCARD_E_UNKNOWN_RES_MNG"          , // _HRESULT_TYPEDEF_(0x8010002BL)
            "SCARD_E_NO_SUCH_CERTIFICATE"      , // _HRESULT_TYPEDEF_(0x8010002CL)
            "SCARD_E_CERTIFICATE_UNAVAILABLE"  , // _HRESULT_TYPEDEF_(0x8010002DL)
            "SCARD_E_NO_READERS_AVAILABLE"     , // _HRESULT_TYPEDEF_(0x8010002EL)
            "SCARD_E_COMM_DATA_LOST"           , // _HRESULT_TYPEDEF_(0x8010002FL)
            "SCARD_E_NO_KEY_CONTAINER"         , // _HRESULT_TYPEDEF_(0x80100030L)
            "SCARD_E_SERVER_TOO_BUSY"          , // _HRESULT_TYPEDEF_(0x80100031L)
            NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,// xxx3x
            NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,// xxx4x
            NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,// xxx5x
            NULL,NULL,NULL,NULL,NULL,// xxx6x
            "SCARD_W_UNSUPPORTED_CARD"         , // _HRESULT_TYPEDEF_(0x80100065L)
            "SCARD_W_UNRESPONSIVE_CARD"        , // _HRESULT_TYPEDEF_(0x80100066L)
            "SCARD_W_UNPOWERED_CARD"           , // _HRESULT_TYPEDEF_(0x80100067L)
            "SCARD_W_RESET_CARD"               , // _HRESULT_TYPEDEF_(0x80100068L)
            "SCARD_W_REMOVED_CARD"             , // _HRESULT_TYPEDEF_(0x80100069L)
            "SCARD_W_SECURITY_VIOLATION"       , // _HRESULT_TYPEDEF_(0x8010006AL)
            "SCARD_W_WRONG_CHV"                , // _HRESULT_TYPEDEF_(0x8010006BL)
            "SCARD_W_CHV_BLOCKED"              , // _HRESULT_TYPEDEF_(0x8010006CL)
            "SCARD_W_EOF"                      , // _HRESULT_TYPEDEF_(0x8010006DL)
            "SCARD_W_CANCELLED_BY_USER"        , // _HRESULT_TYPEDEF_(0x8010006EL)
            "SCARD_W_CARD_NOT_AUTHENTICATED"   , // _HRESULT_TYPEDEF_(0x8010006FL)
            "SCARD_W_CACHE_ITEM_NOT_FOUND"     , // _HRESULT_TYPEDEF_(0x80100070L)
            "SCARD_W_CACHE_ITEM_STALE"         , // _HRESULT_TYPEDEF_(0x80100071L)
            "SCARD_W_CACHE_ITEM_TOO_BIG"       , // _HRESULT_TYPEDEF_(0x80100072L)
        };
        //  Values are 32 bit values laid out as follows:
        //
        //   3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
        //   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
        //  +---+-+-+-----------------------+-------------------------------+
        //  |Sev|C|R|     Facility          |               Code            |
        //  +---+-+-+-----------------------+-------------------------------+
        //
        //  where
        //
        //      Sev - is the severity code
        //
        //          00 - Success
        //          01 - Informational
        //          10 - Warning
        //          11 - Error
        //
        //      C - is the Customer code flag
        //
        //      R - is a reserved bit
        //
        //      Facility - is the facility code
        //
        //      Code - is the facility's status code
        LONG i = (value & 0x0000FFFF);
        if (i < 0 || i > sizeof(names) / sizeof(names[0])) {
            return "<unknown>";
        }
        return names[i];
    }
    /// Конвертирует код возврата функций PC/SC в код возврата функций XFS.
    inline HRESULT translate() {
        switch (value) {/*
            case SCARD_S_SUCCESS:                 return WFS_SUCCESS;               // NO_ERROR
            case SCARD_F_INTERNAL_ERROR:          return WFS_ERR_INTERNAL_ERROR;    // _HRESULT_TYPEDEF_(0x80100001L)
            case SCARD_E_CANCELLED:               return ; // _HRESULT_TYPEDEF_(0x80100002L)
            case SCARD_E_INVALID_HANDLE:          return WFS_ERR_INVALID_HSERVICE;  // _HRESULT_TYPEDEF_(0x80100003L)
            case SCARD_E_INVALID_PARAMETER:       return ; // _HRESULT_TYPEDEF_(0x80100004L)
            case SCARD_E_INVALID_TARGET:          return ; // _HRESULT_TYPEDEF_(0x80100005L)
            case SCARD_E_NO_MEMORY:               return ; // _HRESULT_TYPEDEF_(0x80100006L)
            case SCARD_F_WAITED_TOO_LONG:         return ; // _HRESULT_TYPEDEF_(0x80100007L)
            case SCARD_E_INSUFFICIENT_BUFFER:     return ; // _HRESULT_TYPEDEF_(0x80100008L)
            case SCARD_E_UNKNOWN_READER:          return ; // _HRESULT_TYPEDEF_(0x80100009L)
            case SCARD_E_TIMEOUT:                 return WFS_ERR_TIMEOUT;           // _HRESULT_TYPEDEF_(0x8010000AL)
            case SCARD_E_SHARING_VIOLATION:       return WFS_ERR_LOCKED;            // _HRESULT_TYPEDEF_(0x8010000BL)
            case SCARD_E_NO_SMARTCARD:            return ; // _HRESULT_TYPEDEF_(0x8010000CL)
            case SCARD_E_UNKNOWN_CARD:            return ; // _HRESULT_TYPEDEF_(0x8010000DL)
            case SCARD_E_CANT_DISPOSE:            return ; // _HRESULT_TYPEDEF_(0x8010000EL)
            case SCARD_E_PROTO_MISMATCH:          return ; // _HRESULT_TYPEDEF_(0x8010000FL)
            case SCARD_E_NOT_READY:               return ; // _HRESULT_TYPEDEF_(0x80100010L)
            case SCARD_E_INVALID_VALUE:           return ; // _HRESULT_TYPEDEF_(0x80100011L)
            case SCARD_E_SYSTEM_CANCELLED:        return ; // _HRESULT_TYPEDEF_(0x80100012L)
            case SCARD_F_COMM_ERROR:              return WFS_ERR_HARDWARE_ERROR;    // _HRESULT_TYPEDEF_(0x80100013L)
            case SCARD_F_UNKNOWN_ERROR:           return ; // _HRESULT_TYPEDEF_(0x80100014L)
            case SCARD_E_INVALID_ATR:             return ; // _HRESULT_TYPEDEF_(0x80100015L)
            case SCARD_E_NOT_TRANSACTED:          return ; // _HRESULT_TYPEDEF_(0x80100016L)
            case SCARD_E_READER_UNAVAILABLE:      return WFS_ERR_CONNECTION_LOST;   // _HRESULT_TYPEDEF_(0x80100017L)
            case SCARD_P_SHUTDOWN:                return ; // _HRESULT_TYPEDEF_(0x80100018L)
            case SCARD_E_PCI_TOO_SMALL:           return ; // _HRESULT_TYPEDEF_(0x80100019L)
            case SCARD_E_READER_UNSUPPORTED:      return ; // _HRESULT_TYPEDEF_(0x8010001AL)
            case SCARD_E_DUPLICATE_READER:        return ; // _HRESULT_TYPEDEF_(0x8010001BL)
            case SCARD_E_CARD_UNSUPPORTED:        return ; // _HRESULT_TYPEDEF_(0x8010001CL)
            case SCARD_E_NO_SERVICE:              return WFS_ERR_INTERNAL_ERROR;    // _HRESULT_TYPEDEF_(0x8010001DL)
            case SCARD_E_SERVICE_STOPPED:         return ; // _HRESULT_TYPEDEF_(0x8010001EL)
            case SCARD_E_UNEXPECTED:              return ; // _HRESULT_TYPEDEF_(0x8010001FL)
            case SCARD_E_ICC_INSTALLATION:        return ; // _HRESULT_TYPEDEF_(0x80100020L)
            case SCARD_E_ICC_CREATEORDER:         return ; // _HRESULT_TYPEDEF_(0x80100021L)
            case SCARD_E_UNSUPPORTED_FEATURE:     return ; // _HRESULT_TYPEDEF_(0x80100022L)
            case SCARD_E_DIR_NOT_FOUND:           return ; // _HRESULT_TYPEDEF_(0x80100023L)
            case SCARD_E_FILE_NOT_FOUND:          return ; // _HRESULT_TYPEDEF_(0x80100024L)
            case SCARD_E_NO_DIR:                  return ; // _HRESULT_TYPEDEF_(0x80100025L)
            case SCARD_E_NO_FILE:                 return ; // _HRESULT_TYPEDEF_(0x80100026L)
            case SCARD_E_NO_ACCESS:               return ; // _HRESULT_TYPEDEF_(0x80100027L)
            case SCARD_E_WRITE_TOO_MANY:          return ; // _HRESULT_TYPEDEF_(0x80100028L)
            case SCARD_E_BAD_SEEK:                return ; // _HRESULT_TYPEDEF_(0x80100029L)
            case SCARD_E_INVALID_CHV:             return ; // _HRESULT_TYPEDEF_(0x8010002AL)
            case SCARD_E_UNKNOWN_RES_MNG:         return ; // _HRESULT_TYPEDEF_(0x8010002BL)
            case SCARD_E_NO_SUCH_CERTIFICATE:     return ; // _HRESULT_TYPEDEF_(0x8010002CL)
            case SCARD_E_CERTIFICATE_UNAVAILABLE: return ; // _HRESULT_TYPEDEF_(0x8010002DL)
            case SCARD_E_NO_READERS_AVAILABLE:    return ; // _HRESULT_TYPEDEF_(0x8010002EL)
            case SCARD_E_COMM_DATA_LOST:          return ; // _HRESULT_TYPEDEF_(0x8010002FL)
            case SCARD_E_NO_KEY_CONTAINER:        return ; // _HRESULT_TYPEDEF_(0x80100030L)
            case SCARD_E_SERVER_TOO_BUSY:         return ; // _HRESULT_TYPEDEF_(0x80100031L)
            case SCARD_W_UNSUPPORTED_CARD:        return ; // _HRESULT_TYPEDEF_(0x80100065L)
            case SCARD_W_UNRESPONSIVE_CARD:       return ; // _HRESULT_TYPEDEF_(0x80100066L)
            case SCARD_W_UNPOWERED_CARD:          return ; // _HRESULT_TYPEDEF_(0x80100067L)
            case SCARD_W_RESET_CARD:              return ; // _HRESULT_TYPEDEF_(0x80100068L)
            case SCARD_W_REMOVED_CARD:            return ; // _HRESULT_TYPEDEF_(0x80100069L)
            case SCARD_W_SECURITY_VIOLATION:      return ; // _HRESULT_TYPEDEF_(0x8010006AL)
            case SCARD_W_WRONG_CHV:               return ; // _HRESULT_TYPEDEF_(0x8010006BL)
            case SCARD_W_CHV_BLOCKED:             return ; // _HRESULT_TYPEDEF_(0x8010006CL)
            case SCARD_W_EOF:                     return ; // _HRESULT_TYPEDEF_(0x8010006DL)
            case SCARD_W_CANCELLED_BY_USER:       return ; // _HRESULT_TYPEDEF_(0x8010006EL)
            case SCARD_W_CARD_NOT_AUTHENTICATED:  return ; // _HRESULT_TYPEDEF_(0x8010006FL)
            case SCARD_W_CACHE_ITEM_NOT_FOUND:    return ; // _HRESULT_TYPEDEF_(0x80100070L)
            case SCARD_W_CACHE_ITEM_STALE:        return ; // _HRESULT_TYPEDEF_(0x80100071L)
            case SCARD_W_CACHE_ITEM_TOO_BIG:      return ; // _HRESULT_TYPEDEF_(0x80100072L)
            */
            // Успешно
            case SCARD_S_SUCCESS:           return WFS_SUCCESS;
            // Некорректный хендл карты (hCard)
            case SCARD_E_INVALID_HANDLE:    return WFS_ERR_INVALID_HSERVICE;
            // Ресурсный менеджер подсистемы PC/SC не запущен
            case SCARD_E_NO_SERVICE:        return WFS_ERR_INTERNAL_ERROR;
            // Считыватель был отключен
            case SCARD_E_READER_UNAVAILABLE:return WFS_ERR_CONNECTION_LOST;
            // Кто-то эксклюзивно владеет картой
            case SCARD_E_SHARING_VIOLATION: return WFS_ERR_LOCKED;
            // Внутренняя ошибка взаимодействия с устройством
            case SCARD_F_COMM_ERROR:        return WFS_ERR_HARDWARE_ERROR;
        }
        // TODO: Уточнить тип ошибки, возвращаемой в случае, если трансляция кодов ошибок не удалась.
        return WFS_ERR_INTERNAL_ERROR;
    }
};

template<class OS>
inline OS& operator<<(OS& os, Status s) {
    return os << "status=" << s.name() << " (0x"
              // На каждый байт требуется 2 символа.
              << std::hex << std::setw(2*sizeof(s.value)) << std::setfill('0')
              << s.value << ")";
}

class Result {
    WFSRESULT* pResult;
public:
    Result(REQUESTID ReqID, HSERVICE hService, Status result) {
        init(ReqID, hService, result.translate());
    }
    Result(REQUESTID ReqID, HSERVICE hService, HRESULT result) {
        init(ReqID, hService, result);
    }
    void send(HWND hWnd, DWORD messageType) {
        ::PostMessage(hWnd, messageType, NULL, (LONG)pResult);
    }
private:
    inline void init(REQUESTID ReqID, HSERVICE hService, HRESULT result) {
        // Выделяем память, заполняем ее нулями на всякий случай.
        WFMAllocateBuffer(sizeof(WFSRESULT), WFS_MEM_ZEROINIT, (LPVOID*)&pResult);
        pResult->RequestID = ReqID;
        pResult->hService = hService;
        pResult->hResult = result;
        GetSystemTime(&pResult->tsTimestamp);
    }
};

/** Класс для представления состояния устройств. Позволяет преобразовать состояния в XFS
    состояния и распечатать набор текущих флагов.
*/
class MediaStatus {
    DWORD value;
public:
    MediaStatus() : value(0) {}
    MediaStatus(DWORD value) : value(value) {}
    WORD translateMedia() {
        WORD result = 0;
        // Карта отсутствует в устройстве
        if (value & SCARD_ABSENT) {
            // Media is not present in the device and not at the entering position.
            result |= WFS_IDC_MEDIANOTPRESENT;
        }
        // Карта присутствует в устройстве, однако она не в рабочей позиции
        if (value & SCARD_PRESENT) {
            // Media is present in the device, not in the entering
            // position and not jammed. On the Latched DIP device,
            // this indicates that the card is present in the device and
            // the card is unlatched.
            result |= WFS_IDC_MEDIAPRESENT;
        }
        // Карта присутствует в устройстве, но на ней нет питания
        if (value & SCARD_SWALLOWED) {
            // Media is at the entry/exit slot of a motorized device
            result |= WFS_IDC_MEDIAENTERING;
        }
        // Питание на карту подано, но считыватель не знает режим работы карты
        if (value & SCARD_POWERED) {
            //result |= ;
        }
        // Карта была сброшена (reset) и ожидает согласование PTS.
        if (value & SCARD_NEGOTIABLE) {
            //result |= ;
        }
        // Карта была сброшена (reset) и установлен specific протокол общения.
        if (value & SCARD_SPECIFIC) {
            //result |= ;
        }
        return result;
    }
    WORD translateChipPower() {
        WORD result = 0;
        // Карта отсутствует в устройстве
        if (value & SCARD_ABSENT) {
            // There is no card in the device.
            result |= WFS_IDC_CHIPNOCARD;
        }
        // Карта присутствует в устройстве, однако она не в рабочей позиции
        if (value & SCARD_PRESENT) {
            // The chip is present, but powered off (i.e. not contacted).
            result |= WFS_IDC_CHIPPOWEREDOFF;
        }
        // Карта присутствует в устройстве, но на ней нет питания
        if (value & SCARD_SWALLOWED) {
            // The chip is present, but powered off (i.e. not contacted).
            result |= WFS_IDC_CHIPPOWEREDOFF;
        }
        // Питание на карту подано, но считыватель не знает режим работы карты
        if (value & SCARD_POWERED) {
            //result |= ;
        }
        // Карта была сброшена (reset) и ожидает согласование PTS.
        if (value & SCARD_NEGOTIABLE) {
            // The state of the chip cannot be determined with the device in its current state.
            result |= WFS_IDC_CHIPUNKNOWN;
        }
        // Карта была сброшена (reset) и установлен specific протокол общения.
        if (value & SCARD_SPECIFIC) {
            // The chip is present, powered on and online (i.e.
            // operational, not busy processing a request and not in an
            // error state).
            result |= WFS_IDC_CHIPONLINE;
        }
        return result;
    }

    const std::vector<CTString> flagNames() {
        static CTString names[] = {
            CTString("SCARD_ABSENT"    ),
            CTString("SCARD_PRESENT"   ),
            CTString("SCARD_SWALLOWED" ),
            CTString("SCARD_POWERED"   ),
            CTString("SCARD_NEGOTIABLE"),
            CTString("SCARD_SPECIFIC"  ),
        };
        const std::size_t size = sizeof(names)/sizeof(names[0]);
        std::vector<CTString> result(size);
        int i = 0;
        result[i] = (value & SCARD_ABSENT    ) ? names[i++] : CTString();
        result[i] = (value & SCARD_PRESENT   ) ? names[i++] : CTString();
        result[i] = (value & SCARD_SWALLOWED ) ? names[i++] : CTString();
        result[i] = (value & SCARD_POWERED   ) ? names[i++] : CTString();
        result[i] = (value & SCARD_NEGOTIABLE) ? names[i++] : CTString();
        result[i] = (value & SCARD_SPECIFIC  ) ? names[i++] : CTString();
        return result;
    }
};

class Card {
    /// Хендл карты, с которой будет производиться работа.
    SCARDHANDLE hCard;
    /// Протокол, по которому работает карта.
    DWORD dwActiveProtocol;
    HSERVICE hService;
    std::string readerName;

    // Данный класс будет создавать объекты данного класса, вызывая конструктор.
    friend class PCSC;
private:
    /** Открывает указанную карточку для работы.
    @param hContext Ресурсный менеджер подсистемы PC/SC.
    @param readerName
    */
    Card(HSERVICE hService, SCARDCONTEXT hContext, const char* readerName) : hService(hService), readerName(readerName) {
        Status st = SCardConnect(hContext, readerName, SCARD_SHARE_SHARED,
            // У нас нет предпочитаемого протокола, работаем с тем, что дают
            SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1,
            // Получаем хендл карты и выбранный протокол.
            &hCard, &dwActiveProtocol
        );
        log("Connect to", st);
    }
public:
    ~Card() {
        // При закрытии соединения ничего не делаем с карточкой, оставляем ее в считывателе.
        Status st = SCardDisconnect(hCard, SCARD_LEAVE_CARD);
        log("Disconnect from", st);
    }

    Result lock(REQUESTID ReqID) {
        Status st = SCardBeginTransaction(hCard);
        log("Lock", st);

        return Result(ReqID, hService, st);
    }
    Result unlock(REQUESTID ReqID) {
        // Заканчиваем транзакцию, ничего не делаем с картой.
        Status st = SCardEndTransaction(hCard, SCARD_LEAVE_CARD);
        log("Unlock", st);

        return Result(ReqID, hService, st);
    }

    inline Result result(REQUESTID ReqID, HRESULT result) {
        return Result(ReqID, hService, result);
    }
    void sendResult(HWND hWnd, REQUESTID ReqID, DWORD messageType, HRESULT result) {
        Result(ReqID, hService, result).send(hWnd, messageType);
    }

    LPWFSIDCSTATUS getStatus() {
        // Состояние считывателя.
        MediaStatus state;
        DWORD nameLen;
        DWORD attrLen;
        Status st = SCardStatus(hCard,
            // Имя получать не будем, тем не менее длину получить требуется, NULL недопустим.
            NULL, &nameLen,
            // Небольшой хак допустим, у нас прозрачная обертка, ничего лишнего.
            (DWORD*)&state, &dwActiveProtocol,
            // Атрибуты получать не будем, тем не менее длину получить требуется, NULL недопустим.
            NULL, &attrLen
        );
        log("SCardStatus: ", st);

        if (!st) {
            return NULL;
        }

        WFSIDCSTATUS* lpStatus;
        HRESULT h = WFMAllocateBuffer(sizeof(WFSIDCSTATUS), WFS_MEM_ZEROINIT, (LPVOID*)&lpStatus);
        assert(h >= 0);
        // Набор флагов, определяющих состояние устройства. Наше устройство всегда на связи,
        // т.к. в противном случае при открытии сесии с PC/SC драйвером будет ошибка.
        lpStatus->fwDevice = WFS_IDC_DEVONLINE;
        lpStatus->fwMedia = state.translateMedia();
        // У считывателей нет корзины для захваченных карт.
        lpStatus->fwRetainBin = WFS_IDC_RETAINNOTSUPP;
        // Модуль безопасноси отсутствует
        lpStatus->fwSecurity = WFS_IDC_SECNOTSUPP;
        // Количество возвращенных карт. Так как карта вставляется и вытаскивается руками,
        // то данный параметр не имеет смысла.
        //TODO Хотя, может быть, можно будет его отслеживать как количество вытащенных карт.
        lpStatus->usCards = 0;
        lpStatus->fwChipPower = state.translateChipPower();
        return lpStatus;
    }
    WFSIDCCAPS* getCaps() {
        WFSIDCCAPS* lpCaps;
        HRESULT h = WFMAllocateBuffer(sizeof(WFSIDCCAPS), WFS_MEM_ZEROINIT, (LPVOID*)&lpCaps);
        assert(h >= 0);

        // Устройство является считывателем карт.
        lpCaps->wClass = WFS_SERVICE_CLASS_IDC;
        // Карта вставляется рукой и может быть вытащена в любой момент.
        lpCaps->fwType = WFS_IDC_TYPEDIP;
        // Устройство содержит только считыватель карт.
        lpCaps->bCompound = FALSE;
        // Какие треки могут быть прочитаны -- никакие, только чип.
        lpCaps->fwReadTracks = WFS_IDC_NOTSUPP;
        // Какие треки могут быть записаны -- никакие, только чип.
        lpCaps->fwWriteTracks = WFS_IDC_NOTSUPP;
        // Виды поддерживаемых картой протоколов.
        //lpCaps->fwChipProtocols = WFS_IDC_CHIPT0 | WFS_IDC_CHIPT1;
        // Максимальное количество карт, которое устройство может захватить. Всегда 0, т.к. не захватывает.
        lpCaps->usCards = 0;
        // Тип модуля безопасности. Не поддерживается.
        lpCaps->fwSecType = WFS_IDC_SECNOTSUPP;
        return lpCaps;
    }
private:
    void log(std::string operation, Status st) const {
        std::stringstream ss;
        ss << operation << " card reader '" << readerName << "': " << st;
        WFMOutputTraceData((LPSTR)ss.str().c_str());
    }
};

/** Класс, в конструкторе инициализирующий подсистему PC/SC, а в деструкторе закрывающий ее.
    Необходимо Создать ровно один экземпляр данного класса при загрузке DLL и уничтожить его
    при выгрузке. Наиболее просто это делается, путем объявления глобальной переменной данного
    класса.
*/
class PCSC {
public:
    /// Тип для отображения сервисов XFS на карты PC/SC.
    typedef std::map<HSERVICE, Card*> CardMap;
public:
    /// Контекст подсистемы PC/SC.
    SCARDCONTEXT hContext;
    /// Список карт, открытых для взаимодействия с системой XFS.
    CardMap cards;
public:
    /// Открывает соединение к менеджеру подсистемы PC/SC.
    PCSC() {
        // Создаем контекст.
        Status st = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
        std::stringstream ss;
        ss << "Establish context: " << st;
        WFMOutputTraceData((LPSTR)ss.str().c_str());
    }
    /// Закрывает соединение к менеджеру подсистемы PC/SC.
    ~PCSC() {
        for (CardMap::const_iterator it = cards.begin(); it != cards.end(); ++it) {
            delete it->second;
        }
        cards.clear();
        Status st = SCardReleaseContext(hContext);
        std::stringstream ss;
        ss << "Release context: " << st;
        WFMOutputTraceData((LPSTR)ss.str().c_str());
    }
    /** Проверяет, что указаный хендл сервиса является корректным хендлом карточки. */
    bool isValid(HSERVICE hService) {
        return cards.find(hService) != cards.end();
    }
    Card& open(HSERVICE hService, const char* name) {
        Card* card = new Card(hService, hContext, name);
        cards.insert(std::make_pair(hService, card));
        return *card;
    }
    Card& get(HSERVICE hService) {
        assert(isValid(hService));
        return *cards.find(hService)->second;
    }
    void close(HSERVICE hService) {
        assert(isValid(hService));
        CardMap::iterator it = cards.find(hService);

        delete it->second;
        cards.erase(it);
    }
};

/** При загрузке DLL будут вызваны конструкторы всех глобальных объектов и установится
    соединение с подсистемой PC/SC. При выгрузке DLL вызовутся деструкторы глобальных
    объектов и соединение автоматически закроется.
*/
PCSC pcsc;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

/** Устанавливает сессию с подсистемой PC/SC.

@message WFS_OPEN_COMPLETE

@param hService Хендл сессии, с которым необходимо соотнести хендл открываемой карты.
@param lpszLogicalName Имя логического сервиса, настраивается в реестре HKLM\<user>\XFS\LOGICAL_SERVICES
@param hApp Хендл приложения, созданного вызовом WFSCreateAppHandle.
@param lpszAppID Идентификатор приложения. Может быть `NULL`.
@param dwTraceLevel 
@param dwTimeOut Таймаут (в мс), в течении которого необходимо открыть сервис. `WFS_INDEFINITE_WAIT`,
       если таймаут не требуется.
@param hWnd Окно, которое должно получить сообщение о завершении асинхронной операции.
@param ReqId Идентификатора запроса, который нужно передать окну `hWnd` при завершении операции.
@param hProvider Хендл данного сервис-провайдера. Может быть переден, например, в фукнцию WFMReleaseDLL.
@param dwSPIVersionsRequired Диапазон требуетмых версий провайдера. 0x00112233 означает диапазон
       версий [0.11; 22.33].
@param lpSPIVersion Информация об открытом соединении (выходной параметр).
@param dwSrvcVersionsRequired Специфичная для сервиса (карточный ридер) требуемая версия провайдера.
       0x00112233 означает диапазон версий [0.11; 22.33].
@param lpSrvcVersion Информация об открытом соединении, специфичная для вида провайдера (карточный ридер)
       (выходной параметр).
*/
HRESULT  WINAPI WFPOpen(HSERVICE hService, LPSTR lpszLogicalName, 
                        HAPP hApp, LPSTR lpszAppID, 
                        DWORD dwTraceLevel, DWORD dwTimeOut, 
                        HWND hWnd, REQUESTID ReqID,
                        HPROVIDER hProvider, 
                        DWORD dwSPIVersionsRequired, LPWFSVERSION lpSPIVersion, 
                        DWORD dwSrvcVersionsRequired, LPWFSVERSION lpSrvcVersion
) {
    // Возвращаем поддерживаемые версии.
    if (lpSPIVersion != NULL) {
        // Версия XFS менеджера, которая будет использоваться. Т.к. мы поддерживаем все версии,
        // то просто максимальная запрошенная версия (она в младшем слове).
        lpSPIVersion->wVersion = LOWORD(dwSPIVersionsRequired);
        // Минимальная и максимальная поддерживаемая нами версия
        //TODO: Уточнить минимальную поддерживаемую версию!
        lpSPIVersion->wLowVersion  = MAKE_VERSION(0, 0);
        lpSPIVersion->wHighVersion = MAKE_VERSION(255, 255);
    }
    if (lpSrvcVersion != NULL) {
        // Версия XFS менеджера, которая будет использоваться. Т.к. мы поддерживаем все версии,
        // то просто максимальная запрошенная версия (она в младшем слове).
        lpSrvcVersion->wVersion = LOWORD(dwSrvcVersionsRequired);
        // Минимальная и максимальная поддерживаемая нами версия
        //TODO: Уточнить минимальную поддерживаемую версию!
        lpSrvcVersion->wLowVersion  = MAKE_VERSION(0, 0);
        lpSrvcVersion->wHighVersion = MAKE_VERSION(255, 255);
    }

    // Получаем хендл карты, с которой будем работать.
    pcsc.open(hService, lpszLogicalName).sendResult(hWnd, ReqID, WFS_OPEN_COMPLETE, WFS_SUCCESS);

    // Возможные коды завершения асинхронного запроса (могут возвращаться и другие)
    // WFS_ERR_CANCELED                The request was canceled by WFSCancelAsyncRequest.
    // WFS_ERR_INTERNAL_ERROR          An internal inconsistency or other unexpected error occurred in the XFS subsystem.
    // WFS_ERR_TIMEOUT                 The timeout interval expired.
    // WFS_ERR_VERSION_ERROR_IN_SRVC   Within the service, a version mismatch of two modules occurred.

    // Возможные коды завершения функции:
    // WFS_ERR_CONNECTION_LOST       The connection to the service is lost.
    // WFS_ERR_INTERNAL_ERROR        An internal inconsistency or other unexpected error occurred in the XFS subsystem.
    // WFS_ERR_INVALID_HSERVICE      The hService parameter is not a valid service handle.
    // WFS_ERR_INVALID_HWND          The hWnd parameter is not a valid window handle.
    // WFS_ERR_INVALID_POINTER       A pointer parameter does not point to accessible memory.
    // WFS_ERR_INVALID_TRACELEVEL    The dwTraceLevel parameter does not correspond to a valid trace level or set of levels.
    // WFS_ERR_SPI_VER_TOO_HIGH      The range of versions of XFS SPI support requested by the XFS Manager is higher than any supported by this particular service provider.
    // WFS_ERR_SPI_VER_TOO_LOW       The range of versions of XFS SPI support requested by the XFS Manager is lower than any supported by this particular service provider.
    // WFS_ERR_SRVC_VER_TOO_HIGH     The range of versions of the service-specific interface support requested by the application is higher than any supported by the service provider for the logical service being opened.
    // WFS_ERR_SRVC_VER_TOO_LOW      The range of versions of the service-specific interface support requested by the application is lower than any supported by the service provider for the logical service being opened.
    // WFS_ERR_VERSION_ERROR_IN_SRVC Within the service, a version mismatch of two modules occurred.

    return WFS_SUCCESS;
}
/** Завершает сессию (серию запросов к сервису, инициированную вызовом SPI функции WFPOpen)
    между XFS менеджером и указанным сервис-провайдером.
@par
    Если провайдер заблокирован вызовом WFPLock, он автоматически будет разблокирован и отписан от
    всех событий (если это не сделали ранее вызовом WFPDeregister).
@message WFS_CLOSE_COMPLETE

@param hService Закрываемый сервис-провайдер.
@param hWnd Окно, которое должно получить сообщение о завершении асинхронной операции.
@param ReqId Идентификатора запроса, который нужно передать окну `hWnd` при завершении операции.
*/
HRESULT  WINAPI WFPClose(HSERVICE hService, HWND hWnd, REQUESTID ReqID) {
    if (!pcsc.isValid(hService))
        return WFS_ERR_INVALID_HSERVICE;
    pcsc.close(hService);
    //TODO: Послать окну hWnd сообщение с кодом WFS_CLOSE_COMPLETE и wParam = WFSRESULT;

    // Возможные коды завершения асинхронного запроса (могут возвращаться и другие)
    // WFS_ERR_CANCELED The request was canceled by WFSCancelAsyncRequest.
    // WFS_ERR_INTERNAL_ERROR An internal inconsistency or other unexpected error occurred in the XFS subsystem.

    // Возможные коды завершения функции:
    // WFS_ERR_CONNECTION_LOST The connection to the service is lost.
    // WFS_ERR_INTERNAL_ERROR An internal inconsistency or other unexpected error occurred in the XFS subsystem.
    // WFS_ERR_INVALID_HSERVICE The hService parameter is not a valid service handle.
    // WFS_ERR_INVALID_HWND The hWnd parameter is not a valid window handle.
    return WFS_SUCCESS;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/** Подписывает указанное окно на события указанных классов от указанного сервис-провайдера.

@message WFS_REGISTER_COMPLETE

@param hService Сервис, чьи сообщения требуется мониторить.
@param dwEventClass Логический OR классов сообщений, которые не нужно мониторить. 0 означает, что
       производится подписка на все классы событий.
@param hWndReg Окно, которое будет получать события указанных классов.
@param hWnd Окно, которое должно получить сообщение о завершении асинхронной операции.
@param ReqID Идентификатора запроса, который нужно передать окну `hWnd` при завершении операции.
*/
HRESULT  WINAPI WFPRegister(HSERVICE hService,  DWORD dwEventClass, HWND hWndReg, HWND hWnd, REQUESTID ReqID) {
    if (!pcsc.isValid(hService))
        return WFS_ERR_INVALID_HSERVICE;
    // Возможные коды завершения асинхронного запроса (могут возвращаться и другие)
    // WFS_ERR_CANCELED        The request was canceled by WFSCancelAsyncRequest.
    // WFS_ERR_INTERNAL_ERROR  An internal inconsistency or other unexpected error occurred in the XFS subsystem.

    // Возможные коды завершения функции:
    // WFS_ERR_CONNECTION_LOST     The connection to the service is lost.
    // WFS_ERR_INTERNAL_ERROR      An internal inconsistency or other unexpected error occurred in the XFS subsystem.
    // WFS_ERR_INVALID_EVENT_CLASS The dwEventClass parameter specifies one or more event classes not supported by the service.
    // WFS_ERR_INVALID_HSERVICE    The hService parameter is not a valid service handle.
    // WFS_ERR_INVALID_HWND        The hWnd parameter is not a valid window handle.
    // WFS_ERR_INVALID_HWNDREG     The hWndReg parameter is not a valid window handle.
    return WFS_SUCCESS;
}
/** Прерывает мониторинг указанных классов сообщений от указанного сервис-провайдера для указанных окон.

@message WFS_DEREGISTER_COMPLETE

@param hService Сервис, чьи сообщения больше не требуется мониторить.
@param dwEventClass Логический OR классов сообщений, которые не нужно мониторить. 0 означает, что отписка
       производится от всех классов событий.
@param hWndReg Окно, которое больше не должно получать указанные сообщения. NULL означает, что отписка
       производится для всех окон.
@param hWnd Окно, которое должно получить сообщение о завершении асинхронной операции.
@param ReqID Идентификатора запроса, который нужно передать окну `hWnd` при завершении операции.
*/
HRESULT  WINAPI WFPDeregister(HSERVICE hService, DWORD dwEventClass, HWND hWndReg, HWND hWnd, REQUESTID ReqID) {
    if (!pcsc.isValid(hService))
        return WFS_ERR_INVALID_HSERVICE;
    // Возможные коды завершения асинхронного запроса (могут возвращаться и другие)
    // WFS_ERR_CANCELED The request was canceled by WFSCancelAsyncRequest.

    // Возможные коды завершения функции:
    // WFS_ERR_CONNECTION_LOST     The connection to the service is lost.
    // WFS_ERR_INTERNAL_ERROR      An internal inconsistency or other unexpected error occurred in the XFS subsystem.
    // WFS_ERR_INVALID_EVENT_CLASS The dwEventClass parameter specifies one or more event classes not supported by the service.
    // WFS_ERR_INVALID_HSERVICE    The hService parameter is not a valid service handle.
    // WFS_ERR_INVALID_HWND        The hWnd parameter is not a valid window handle.
    // WFS_ERR_INVALID_HWNDREG     The hWndReg parameter is not a valid window handle.
    // WFS_ERR_NOT_REGISTERED      The specified hWndReg window was not registered to receive messages for any event classes.
    return WFS_SUCCESS;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/** Получает эксклюзивный доступ к устройству.

@message WFS_CLOSE_COMPLETE

@param hService Сервис, к которому нужно получить эксклюзивный доступ.
@param dwTimeOut Таймаут (в мс), в течении которого необходимо получить доступ. `WFS_INDEFINITE_WAIT`,
       если таймаут не требуется.
@param hWnd Окно, которое должно получить сообщение о завершении асинхронной операции.
@param ReqID Идентификатора запроса, который нужно передать окну `hWnd` при завершении операции.
*/
HRESULT  WINAPI WFPLock(HSERVICE hService, DWORD dwTimeOut, HWND hWnd, REQUESTID ReqID) {
    if (!pcsc.isValid(hService))
        return WFS_ERR_INVALID_HSERVICE;
    pcsc.get(hService).lock(ReqID).send(hWnd, WFS_SUCCESS);

    // Возможные коды завершения асинхронного запроса (могут возвращаться и другие)
    // WFS_ERR_CANCELED        The request was canceled by WFSCancelAsyncRequest.
    // WFS_ERR_DEV_NOT_READY   The function required device access, and the device was not ready or timed out.
    // WFS_ERR_HARDWARE_ERROR  The function required device access, and an error occurred on the device.
    // WFS_ERR_INTERNAL_ERROR  An internal inconsistency or other unexpected error occurred in the XFS subsystem.
    // WFS_ERR_SOFTWARE_ERROR  The function required access to configuration information, and an error occurred on the software.
    // WFS_ERR_TIMEOUT         The timeout interval expired.

    // Возможные коды завершения функции:
    // WFS_ERR_CONNECTION_LOST    The connection to the service is lost.
    // WFS_ERR_INTERNAL_ERROR     An internal inconsistency or other unexpected error occurred in the XFS subsystem.
    // WFS_ERR_INVALID_HSERVICE   The hService parameter is not a valid service handle.
    // WFS_ERR_INVALID_HWND       The hWnd parameter is not a valid window handle.
    return WFS_SUCCESS;
}
/**
@message WFS_UNLOCK_COMPLETE

@param hService Сервис, к которому больше ну нужно иметь эксклюзивный доступ.
@param hWnd Окно, которое должно получить сообщение о завершении асинхронной операции.
@param ReqID Идентификатора запроса, который нужно передать окну `hWnd` при завершении операции.
*/
HRESULT  WINAPI WFPUnlock(HSERVICE hService, HWND hWnd, REQUESTID ReqID) {
    if (!pcsc.isValid(hService))
        return WFS_ERR_INVALID_HSERVICE;

    pcsc.get(hService).unlock(ReqID).send(hWnd, WFS_SUCCESS);
    // Возможные коды завершения асинхронного запроса (могут возвращаться и другие)
    // WFS_ERR_CANCELED        The request was canceled by WFSCancelAsyncRequest.
    // WFS_ERR_INTERNAL_ERROR  An internal inconsistency or other unexpected error occurred in the XFS subsystem.
    // WFS_ERR_NOT_LOCKED      The service to be unlocked is not locked under the calling hService.

    // Возможные коды завершения функции:
    // WFS_ERR_CONNECTION_LOST    The connection to the service is lost.
    // WFS_ERR_INTERNAL_ERROR     An internal inconsistency or other unexpected error occurred in the XFS subsystem.
    // WFS_ERR_INVALID_HSERVICE   The hService parameter is not a valid service handle.
    // WFS_ERR_INVALID_HWND       The hWnd parameter is not a valid window handle.

    //TODO: Послать окну hWnd сообщение с кодом WFS_UNLOCK_COMPLETE и wParam = WFSRESULT;
    return WFS_SUCCESS;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

/** Получает различную информацию о сервис-провайдере.

@message WFS_GETINFO_COMPLETE

@param hService Хендл сервис-провайдера, о котором получается информация.
@param dwCategory Вид запрашиваемой информации. Поддерживаются только стандартные виды информации.
@param pQueryDetails Указатель на дополнительные данные для запроса или `NULL`, если их нет.
@param dwTimeOut Таймаут, за который нужно завершить операцию.
@param hWnd Окно, которое должно получить сообщение о завершении асинхронной операции.
@param ReqID Идентификатора запроса, который нужно передать окну `hWnd` при завершении операции.
*/
HRESULT  WINAPI WFPGetInfo(HSERVICE hService, DWORD dwCategory, LPVOID lpQueryDetails, DWORD dwTimeOut, HWND hWnd, REQUESTID ReqID) {
    if (!pcsc.isValid(hService))
        return WFS_ERR_INVALID_HSERVICE;
    // Для IDC могут запрашиваться только эти константы (WFS_INF_IDC_*)
    switch (dwCategory) {
        case WFS_INF_IDC_STATUS: {// Дополнительных параметров нет
            pcsc.get(hService).getStatus();
            break;
        }
        case WFS_INF_IDC_CAPABILITIES: {

            //LPWFSIDCCAPS st = (LPWFSIDCCAPS)lpQueryDetails;
            break;
        }
        case WFS_INF_IDC_FORM_LIST:
        case WFS_INF_IDC_QUERY_FORM: {
            // Формы не поддерживаем. Что это за формы такие -- непонятно.
            return WFS_ERR_UNSUPP_COMMAND;
        }
        default:
            return WFS_ERR_INVALID_CATEGORY;
    }
    // Возможные коды завершения асинхронного запроса (могут возвращаться и другие)
    // WFS_ERR_CANCELED        The request was canceled by WFSCancelAsyncRequest.
    // WFS_ERR_DEV_NOT_READY   The function required device access, and the device was not ready or timed out.
    // WFS_ERR_HARDWARE_ERROR  The function required device access, and an error occurred on the device.
    // WFS_ERR_INTERNAL_ERROR  An internal inconsistency or other unexpected error occurred in the XFS subsystem.
    // WFS_ERR_INVALID_DATA    The data structure passed as input parameter contains invalid data..
    // WFS_ERR_SOFTWARE_ERROR  The function required access to configuration information, and an error occurred on the software.
    // WFS_ERR_TIMEOUT         The timeout interval expired.
    // WFS_ERR_USER_ERROR      A user is preventing proper operation of the device.
    // WFS_ERR_UNSUPP_DATA     The data structure passed as an input parameter although valid for this service class, is not supported by this service provider or device.

    // Возможные коды завершения функции:
    // WFS_ERR_CONNECTION_LOST    The connection to the service is lost.
    // WFS_ERR_INTERNAL_ERROR     An internal inconsistency or other unexpected error occurred in the XFS subsystem.
    // WFS_ERR_INVALID_CATEGORY   The dwCategory issued is not supported by this service class.
    // WFS_ERR_INVALID_HSERVICE   The hService parameter is not a valid service handle.
    // WFS_ERR_INVALID_HWND       The hWnd parameter is not a valid window handle.
    // WFS_ERR_INVALID_POINTER    A pointer parameter does not point to accessible memory.
    // WFS_ERR_UNSUPP_CATEGORY    The dwCategory issued, although valid for this service class, is not supported by this service provider.
    //TODO: Послать окну hWnd сообщение с кодом WFS_GETINFO_COMPLETE и wParam = WFSRESULT;
    return WFS_SUCCESS;
}
/** Посылает команду для выполнения карточным ридером.

@message WFS_EXECUTE_COMPLETE

@param hService Хендл сервис-провайдера, который должен выполнить команду.
@param dwCommand Вид выполняемой команды.
@param lpCmdData Даные для команды. Зависят от вида.
@param dwTimeOut Количество миллисекунд, за которое команда должна выполниться, или `WFS_INDEFINITE_WAIT`,
       если таймаут не требуется.
@param hWnd Окно, которое должно получить сообщение о завершении асинхронной операции.
@param ReqID Идентификатора запроса, который нужно передать окну `hWnd` при завершении операции.
*/
HRESULT  WINAPI WFPExecute(HSERVICE hService, DWORD dwCommand, LPVOID lpCmdData, DWORD dwTimeOut, HWND hWnd, REQUESTID ReqID) {
    if (!pcsc.isValid(hService))
        return WFS_ERR_INVALID_HSERVICE;
    //Status st = SCardTransmit(translate(hService), );
    // Возможные коды завершения асинхронного запроса (могут возвращаться и другие)
    // WFS_ERR_CANCELED        The request was canceled by WFSCancelAsyncRequest.
    // WFS_ERR_DEV_NOT_READY   The function required device access, and the device was not ready or timed out.
    // WFS_ERR_HARDWARE_ERROR  The function required device access, and an error occurred on the device.
    // WFS_ERR_INVALID_DATA    The data structure passed as input parameter contains invalid data..
    // WFS_ERR_INTERNAL_ERROR  An internal inconsistency or other unexpected error occurred in the XFS subsystem.
    // WFS_ERR_LOCKED          The service is locked under a different hService.
    // WFS_ERR_SOFTWARE_ERROR  The function required access to configuration information, and an error occurred on the software.
    // WFS_ERR_TIMEOUT         The timeout interval expired.
    // WFS_ERR_USER_ERROR      A user is preventing proper operation of the device.
    // WFS_ERR_UNSUPP_DATA     The data structure passed as an input parameter although valid for this service class, is not supported by this service provider or device.

    // Возможные коды завершения функции:
    // WFS_ERR_CONNECTION_LOST    The connection to the service is lost.
    // WFS_ERR_INTERNAL_ERROR     An internal inconsistency or other unexpected error occurred in the XFS subsystem.
    // WFS_ERR_INVALID_COMMAND    The dwCommand issued is not supported by this service class.
    // WFS_ERR_INVALID_HSERVICE   The hService parameter is not a valid service handle.
    // WFS_ERR_INVALID_HWND       The hWnd parameter is not a valid window handle.
    // WFS_ERR_INVALID_POINTER    A pointer parameter does not point to accessible memory.
    // WFS_ERR_UNSUPP_COMMAND     The dwCommand issued, although valid for this service class, is not supported by this service provider.
    return WFS_SUCCESS;
}
/** Отменяет указанный (либо все) асинхронный запрос, выполняемый провайдером, прежде, чем он завершится.
    Все запросы, котрые не успели выпонится к этому времени, завершатся с кодом `WFS_ERR_CANCELED`.

    Отмена запросов не поддерживается.
@param hService
@param ReqID Идентификатор запроса для отмены или `NULL`, если необходимо отменить все запросы
       для указанного сервися `hService`.
*/
HRESULT  WINAPI WFPCancelAsyncRequest(HSERVICE hService, REQUESTID ReqID) {
    if (!pcsc.isValid(hService))
        return WFS_ERR_INVALID_HSERVICE;
    // Возможные коды завершения функции:
    // WFS_ERR_CONNECTION_LOST //The connection to the service is lost.
    // WFS_ERR_INTERNAL_ERROR //An internal inconsistency or other unexpected error occurred in the XFS subsystem.
    // WFS_ERR_INVALID_HSERVICE //The hService parameter is not a valid service handle.
    // WFS_ERR_INVALID_REQ_ID //The RequestID parameter does not correspond to an outstanding request on the service.
    return WFS_ERR_UNSUPP_COMMAND;
}
HRESULT  WINAPI WFPSetTraceLevel(HSERVICE hService, DWORD dwTraceLevel) {
    if (!pcsc.isValid(hService))
        return WFS_ERR_INVALID_HSERVICE;
    // Возможные коды завершения функции:
    // WFS_ERR_CONNECTION_LOST    The connection to the service is lost.
    // WFS_ERR_INTERNAL_ERROR     An internal inconsistency or other unexpected error occurred in the XFS subsystem.
    // WFS_ERR_INVALID_HSERVICE   The hService parameter is not a valid service handle.
    // WFS_ERR_INVALID_TRACELEVEL The dwTraceLevel parameter does not correspond to a valid trace level or set of levels.
    // WFS_ERR_NOT_STARTED        The application has not previously performed a successful WFSStartUp.
    // WFS_ERR_OP_IN_PROGRESS     A blocking operation is in progress on the thread; only WFSCancelBlockingCall and WFSIsBlocking are permitted at this time.
    return WFS_SUCCESS;
}
/** Вызывается XFS для поределения того, можно ли выгрузить DLL с данным сервис-провайдером прямо сейчас. */
HRESULT  WINAPI WFPUnloadService() {
    // Возможные коды завершения функции:
    // WFS_ERR_NOT_OK_TO_UNLOAD
    //     The XFS Manager may not unload the service provider DLL at this time. It will repeat this
    //     request to the service provider until the return is WFS_SUCCESS, or until a new session is
    //     started by an application with this service provider.

    // Нас всегда можно выгрузить.
    return WFS_SUCCESS;
}