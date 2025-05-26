#include "Settings.h"

#include "XFS/Logger.h"

#include <string>
#include <vector>
// API XFS pour les fonctions d'accès au registre.
#include <xfsconf.h>

/// Classe pour fermer automatiquement les clés de registre ouvertes lorsqu'elles ne sont plus nécessaires.
class RegKey {
    HKEY hKey;
public:
    inline RegKey(HKEY root, const char* name) {
        HRESULT r = WFMOpenKey(root, (LPSTR)name, &hKey);

        XFS::Logger() << "RegKey::RegKey(root=" << root << ", name=" << name << ", hKey=&" << hKey << ") = "  << r;
    }
    inline ~RegKey() {
        HRESULT r = WFMCloseKey(hKey);

        XFS::Logger() << "WFMCloseKey(hKey=" << hKey << ") = " << r;
    }

    inline RegKey child(const char* name) const {
        return RegKey(hKey, name);
    }
    /** Obtient la valeur de la clé de registre avec le nom spécifié.
    @param name
        Nom de la valeur dans la clé de registre à obtenir. La valeur par défaut (`NULL`)
        signifie que la valeur par défaut de la clé doit être obtenue.

    @return
        Chaîne contenant la valeur de la clé. Si la valeur n'existe pas, retourne une chaîne vide.
    */
    inline std::string value(const char* name = NULL) const {
        // Détermine la taille de la valeur de la clé.
        DWORD dwSize = 0;
        HRESULT r = WFMQueryValue(hKey, (LPSTR)name, NULL, &dwSize);

        {XFS::Logger() << "RegKey::value[size](name=" << name << ", size=&" << dwSize << ") = " << r;}
        // Utilise un vecteur car il garantit la continuité de la mémoire pour les données,
        // ce qui n'est pas le cas avec string.
        // dwSize contient la longueur de la chaîne sans le NULL final, mais il est écrit dans la valeur de sortie.
        std::vector<char> value(dwSize+1);
        if (dwSize > 0) {
            dwSize = value.capacity();
            r = WFMQueryValue(hKey, (LPSTR)name, &value[0], &dwSize);
            {XFS::Logger() << "RegKey::value[value](name=" << name << ", value=&" << &value[0] << ", size=&" << dwSize << ") = " << r;}
        }
        std::string result = std::string(value.begin(), value.end()-1);

        XFS::Logger() << "RegKey::value(name=" << name << ") = " << result;
        return result;
    }
    inline DWORD dwValue(const char* name) const {
        // Détermine la taille de la valeur de la clé.
        DWORD result = 0;
        DWORD dwSize = sizeof(DWORD);
        HRESULT r = WFMQueryValue(hKey, (LPSTR)name, (LPSTR)&result, &dwSize);

        XFS::Logger() << "RegKey::value(name=" << name << ") = " << result;
        return result;
    }
    /// Fonction de débogage pour afficher dans la trace toutes les clés enfants.
    void keys() const {
        {XFS::Logger() << "keys";}
        std::vector<char> keyName(256);
        for (DWORD i = 0; ; ++i) {
            DWORD size = keyName.capacity();
            HRESULT r = WFMEnumKey(hKey, i, &keyName[0], &size, NULL);
            if (r == WFS_ERR_CFG_NO_MORE_ITEMS) {
                break;
            }
            keyName[size] = '\0';

            XFS::Logger() << &keyName[0];
        }
    }
    /// Fonction de débogage pour afficher dans la trace toutes les valeurs enfants de la clé.
    /// Une valeur de clé est une paire (nom=valeur).
    void values() const {
        {XFS::Logger() << "values";}
        // Malheureusement, il est impossible de connaître les longueurs spécifiques à l'avance.
        std::vector<char> name(256);
        std::vector<char> value(256);
        for (DWORD i = 0; ; ++i) {
            DWORD szName = name.capacity();
            DWORD szValue = value.capacity();
            HRESULT r = WFMEnumValue(hKey, i, &name[0], &szName, &value[0], &szValue);
            if (r == WFS_ERR_CFG_NO_MORE_ITEMS) {
                break;
            }
            name[szName] = '\0';
            value[szValue] = '\0';

            XFS::Logger()
                << i << ": " << '('<<szName<<','<<szValue<<')' << std::string(name.begin(), name.begin()+szName) << "="
                << std::string(value.begin(), value.begin()+szValue);
        }
    }
};

Settings::Settings(const char* serviceName, int traceLevel)
    : traceLevel(traceLevel)
    , exclusive(false)
{
    // Chez Kalignite, le fournisseur n'apparaît pas sous cette racine s'il est dans
    // HKEY_LOCAL_MACHINE\SOFTWARE\XFS\SERVICE_PROVIDERS\
    // HKEY root = WFS_CFG_HKEY_XFS_ROOT;
    HKEY root = WFS_CFG_USER_DEFAULT_XFS_ROOT;// HKEY_USERS\.DEFAULT\XFS
    providerName = RegKey(root, "LOGICAL_SERVICES").child(serviceName).value("provider");

    reread();
}
void Settings::reread() {
    HKEY root = WFS_CFG_USER_DEFAULT_XFS_ROOT;// HKEY_USERS\.DEFAULT\XFS

    RegKey pcscSettings = RegKey(root, "SERVICE_PROVIDERS").child(providerName.c_str());
    readerName = pcscSettings.value("ReaderName");
    traceLevel = pcscSettings.dwValue("TraceLevel");
    exclusive  = pcscSettings.dwValue("Exclusive") != 0;

    // Paramètres pour contourner divers problèmes
    RegKey workaroundSettings = pcscSettings.child("Workarounds");
    workarounds.correctChipIO = workaroundSettings.dwValue("CorrectChipIO") != 0;
    workarounds.canEject = workaroundSettings.dwValue("CanEject") != 0;

    RegKey track2Settings = workaroundSettings.child("Track2");
    workarounds.track2.report = track2Settings.dwValue("Report") != 0;
    workarounds.track2.value = track2Settings.value();

    XFS::Logger() << "Settings::reread: Nouveaux paramètres lus : " << toJSONString();
}
std::string Settings::toJSONString() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "\tProviderName: " << providerName << ",\n";
    ss << "\tReaderName: " << readerName << ",\n";
    ss << "\tTraceLevel: " << traceLevel << ",\n";
    ss << "\tExclusive: " << std::boolalpha << exclusive << ",\n";
    ss << "\tWorkarounds.CorrectChipIO: " << std::boolalpha << workarounds.correctChipIO << ",\n";
    ss << "\tWorkarounds.CanEject: " << std::boolalpha << workarounds.canEject << ",\n";
    ss << "\tWorkarounds.Track2.Report: " << std::boolalpha << workarounds.track2.report << ",\n";
    ss << "\tWorkarounds.Track2.Value: " << workarounds.track2.value << ",\n";
    ss << '}';
    return ss.str();
}