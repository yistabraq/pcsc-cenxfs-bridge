#ifndef PCSC_CENXFS_BRIDGE_Utils_H
#define PCSC_CENXFS_BRIDGE_Utils_H

#pragma once

#include <vector>
// Для std::size_t
#include <cstddef>
#include <cassert>
// Для WFMAllocateBuffer
#include <xfsadmin.h>

/// Класс строки времени компиляции. Позволяет эффективно получать длину строки
/// от строк, заданных в коде.
class CTString {
    const char* mBegin;
    const char* mEnd;
public:
    inline CTString() : mBegin(0), mEnd(0) {}
    template<std::size_t N>
    inline CTString(const char (&data)[N]) : mBegin(data), mEnd(data + N) {}

    inline std::size_t size() const { return mEnd - mBegin; }
    inline bool empty() const { return size() == 0; }
    inline bool isValid() const { return mBegin != 0; }
    inline const char* begin() const { return mBegin; }
    inline const char* end() const { return mEnd; }
};

template<class OS>
inline OS& operator<<(OS& os, CTString s) {
    if (s.isValid())
        return os << s.begin();
    return os;
}
/** Функция, выделяющая память под структуру указанного типа менеджером памяти XFS.
    Память, выделенная данной функцие, должна быть освобождена вызовом функции
    `WFMFreeBuffer` (если указатель отдается вызывающему, это делает клиент DLL).
@tparam T Тип структуры, для которой выделяется память. Конструктор не вызывается.
*/
template<typename T>
static T* xfsAlloc() {
    T* result = 0;
    HRESULT h = WFMAllocateBuffer(sizeof(T), WFS_MEM_ZEROINIT, (void**)&result);
    assert(h >= 0 && "Cannot allocate memory");
    return result;
}
template<typename T>
static T* xfsAlloc(const T& value) {
    T* result = 0;
    HRESULT h = WFMAllocateBuffer(sizeof(T), WFS_MEM_ZEROINIT, (void**)&result);
    assert(h >= 0 && "Cannot allocate memory");
    *result = value;
    return result;
}


template<typename T, class Derived>
class Enum {
    static const T defaultMask = ~((T)0);
protected:
    T mValue;
    template<class OS>
    friend inline OS& operator<<(OS& os, Enum<T, Derived> e) {
        return os << e.derived().name() << " (0x"
                  // На каждый байт требуется 2 символа.
                  << std::hex << std::setw(2*sizeof(e.mValue)) << std::setfill('0')
                  << e.mValue << ")";
    }
protected:
    Enum(T value) : mValue(value) {}

public:
    inline T value() const { return mValue; }
protected:
    bool name(const CTString* names, std::size_t begin, std::size_t end, T mask, CTString& result) const {
        T val = mValue & mask;
        if (val < (T)begin || val > (T)end) {
            result = CTString("<unknown>");
            return false;
        }
        result = names[val];
        return true;
    }
    inline bool name(const CTString* names, std::size_t begin, std::size_t end, CTString& result) const {
        return name(names, begin, end, defaultMask, result);
    }
    template<std::size_t N>
    inline bool name(CTString (&names)[N], T mask, CTString& result) const {
        return name(names, 0, N, mask, result);
    }
    template<std::size_t N>
    inline bool name(CTString (&names)[N], CTString& result) const {
        return name(names, 0, N, defaultMask, result);
    }
    template<std::size_t N>
    inline CTString name(CTString (&names)[N], T mask) const {
        CTString result;
        name(names, 0, N, mask, result);
        return result;
    }
    template<std::size_t N>
    inline CTString name(CTString (&names)[N]) const {
        CTString result;
        name(names, 0, N, defaultMask, result);
        return result;
    }
private:
    Derived& derived() const { return *((Derived*)this); }
};

template<typename T, class Derived>
class Flags {
protected:
    T mValue;
    template<class OS>
    friend inline OS& operator<<(OS& os, Flags<T, Derived> f) {
        os << "0x" << std::hex << std::setfill('0') << std::setw(2*sizeof(f.mValue)) << " (";
        std::vector<CTString> names = f.derived().flagNames();
        bool first = true;
        for (std::vector<CTString>::const_iterator it = names.begin(); it != names.end(); ++it) {
            // Пустые значения не добавляем в список.
            if (!it->isValid())
                continue;
            if (!first) {
                os << ", ";
            }
            os << *it;
            first = false;
        }
        return os << ')';
    }
protected:
    Flags(T value) : mValue(value) {}
public:
    inline T value() const { return mValue; }
protected:
    template<std::size_t N>
    inline std::vector<CTString> flagNames(CTString (&names)[N]) const {
        std::vector<CTString> result(N);
        if (mValue == (T)0) {
            result[0] = names[0];
        }
        for (std::size_t i = 1; i < N; ++i) {
            if (mValue & (1 << i)) {
                result[i] = names[i];
            }
        }
        return result;
    }
private:
    Derived& derived() const { return *((Derived*)this); }
};

#endif // PCSC_CENXFS_BRIDGE_Utils_H