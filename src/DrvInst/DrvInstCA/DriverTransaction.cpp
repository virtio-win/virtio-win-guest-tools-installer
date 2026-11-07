#include "stdafx.h"
#include "DriverTransaction.h"
#include <initguid.h>
#include <devpkey.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "advapi32.lib")

#define DRIVER_TRY(expr) do { HRESULT result = (expr); if (FAILED(result)) return result; } while (0)

struct DriverDevices
{
    HDEVINFO handle;
    explicit DriverDevices(HDEVINFO value) : handle(value) { }
    DriverDevices(const DriverDevices&) = delete;
    DriverDevices& operator=(const DriverDevices&) = delete;
    operator HDEVINFO() const { return handle; }
    void Close()
    {
        if (handle != INVALID_HANDLE_VALUE) SetupDiDestroyDeviceInfoList(handle);
        handle = INVALID_HANDLE_VALUE;
    }
    ~DriverDevices() { Close(); }
};

static HRESULT DriverLastError()
{
    DWORD error = GetLastError();
    return HRESULT_FROM_WIN32(error ? error : ERROR_GEN_FAILURE);
}

static HRESULT HashFile(PCWSTR path, std::wstring& digest)
{
    digest.clear();
    digest.reserve(64);
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) return DriverLastError();
    BCRYPT_ALG_HANDLE algorithm = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    NTSTATUS status = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, NULL, 0);
    if (status >= 0) status = BCryptCreateHash(algorithm, &hash, NULL, 0, NULL, 0, 0);
    HRESULT hr = status < 0 ? HRESULT_FROM_NT(status) : S_OK;
    BYTE buffer[16384], result[32];
    DWORD read = 0;
    while (SUCCEEDED(hr))
    {
        if (!ReadFile(file, buffer, sizeof(buffer), &read, NULL)) { hr = DriverLastError(); break; }
        if (!read) break;
        status = BCryptHashData(hash, buffer, read, 0);
        if (status < 0) hr = HRESULT_FROM_NT(status);
    }
    if (SUCCEEDED(hr))
    {
        status = BCryptFinishHash(hash, result, sizeof(result), 0);
        if (status < 0) hr = HRESULT_FROM_NT(status);
        else
        {
            digest.clear();
            for (BYTE value : result)
            {
                digest.push_back(L"0123456789abcdef"[value >> 4]);
                digest.push_back(L"0123456789abcdef"[value & 15]);
            }
        }
    }
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    CloseHandle(file);
    return hr;
}

static HRESULT PackageIdentity(PCWSTR inf, DriverPackageIdentity& package)
{
    DWORD size = 0;
    if (!SetupGetInfInformationW(inf, INFINFO_INF_NAME_IS_ABSOLUTE, NULL, 0, &size))
        return DriverLastError();
    std::vector<BYTE> bytes(size);
    auto information = reinterpret_cast<PSP_INF_INFORMATION>(bytes.data());
    if (!SetupGetInfInformationW(inf, INFINFO_INF_NAME_IS_ABSOLUTE, information, size, &size))
        return DriverLastError();
    SP_ORIGINAL_FILE_INFO_W original = { sizeof(original) };
    if (!SetupQueryInfOriginalFileInformationW(information, 0, NULL, &original)) return DriverLastError();
    package.storeInf = inf;
    DRIVER_TRY(HashFile(inf, package.infHash));
    std::wstring catalog(inf);
    catalog = catalog.substr(0, catalog.find_last_of(L"\\/") + 1) + original.OriginalCatalogName;
    HRESULT hr = *original.OriginalCatalogName ? HashFile(catalog.c_str(), package.catalogHash) :
        HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) || hr == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND))
    {
        // Inbox packages may keep their catalog in the system catalog database, not beside the INF.
        SP_INF_SIGNER_INFO_W signer = { sizeof(signer) };
        if (!SetupVerifyInfFileW(inf, NULL, &signer)) return DriverLastError();
        if (!*signer.CatalogFile) return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
        return HashFile(signer.CatalogFile, package.catalogHash);
    }
    return hr;
}

static HRESULT StoreLocation(PCWSTR oem, std::vector<wchar_t>& path)
{
    DWORD needed = 0;
    SetupGetInfDriverStoreLocationW(oem, NULL, NULL, NULL, 0, &needed);
    if (!needed) return DriverLastError();
    path.resize(needed);
    if (!SetupGetInfDriverStoreLocationW(oem, NULL, NULL, path.data(), needed, &needed))
        return DriverLastError();
    return S_OK;
}

static HRESULT PublishedIdentity(PCWSTR oem, DriverPackageIdentity& package)
{
    std::vector<wchar_t> path;
    DRIVER_TRY(StoreLocation(oem, path));
    return PackageIdentity(path.data(), package);
}

static HRESULT ResolvePublishedPackage(const DriverPackageIdentity& expected,
    std::wstring& published, DriverPackageIdentity& actual, PCWSTR aliasHint = L"")
{
    HRESULT scanError = S_OK;
    auto lookup = [&](const std::wstring& hint, DriverPublishedPackage& candidate) {
        DriverPackageIdentity location;
        if (FAILED(PublishedIdentity(hint.c_str(), location))) return false;
        DWORD needed = 0;
        SetupGetInfPublishedNameW(location.storeInf.c_str(), NULL, 0, &needed);
        if (!needed) return false;
        std::vector<wchar_t> name(needed);
        if (!SetupGetInfPublishedNameW(location.storeInf.c_str(), name.data(), needed, &needed)) return false;
        DriverPackageIdentity verified;
        if (FAILED(PublishedIdentity(name.data(), verified))) return false;
        candidate = { name.data(), verified };
        return true;
    };
    auto scan = [&](std::vector<DriverPublishedPackage>& candidates) {
        DWORD needed = 0;
        SetupGetInfFileListW(NULL, INF_STYLE_WIN4, NULL, 0, &needed);
        if (!needed) { scanError = DriverLastError(); return false; }
        std::vector<wchar_t> files(needed);
        if (!SetupGetInfFileListW(NULL, INF_STYLE_WIN4, files.data(), needed, &needed))
        { scanError = DriverLastError(); return false; }
        wchar_t windows[MAX_PATH];
        UINT count = GetWindowsDirectoryW(windows, _countof(windows));
        if (!count || count >= _countof(windows)) { scanError = HRESULT_FROM_WIN32(ERROR_INVALID_DATA); return false; }
        for (PCWSTR name = files.data(); *name; name += wcslen(name) + 1)
        {
            std::wstring path = std::wstring(windows) + L"\\INF\\" + name;
            std::wstring hash;
            HRESULT hr = HashFile(path.c_str(), hash);
            if (FAILED(hr)) { scanError = hr; return false; }
            if (hash != expected.infHash) continue;
            DriverPublishedPackage candidate;
            if (!lookup(path, candidate)) { scanError = HRESULT_FROM_WIN32(ERROR_INVALID_DATA); return false; }
            candidates.push_back(candidate);
        }
        return true;
    };
    DriverPublishedPackage resolved;
    DriverResolution result = ResolveDriverPackageIdentity(expected, { expected.storeInf, aliasHint }, lookup, scan, resolved);
    if (result == DriverResolution::missing) return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    if (result == DriverResolution::ambiguous) return HRESULT_FROM_WIN32(ERROR_DUP_NAME);
    if (result == DriverResolution::invalidIdentity) return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    if (result == DriverResolution::readError) return FAILED(scanError) ? scanError : E_FAIL;
    published = resolved.published;
    actual = resolved.identity;
    return S_OK;
}

static HRESULT VerifyDriverSource(PCWSTR source)
{
    SP_INF_SIGNER_INFO_W signer = { sizeof(signer) };
    if (SetupVerifyInfFileW(source, NULL, &signer)) return S_OK;
    HRESULT hr = DriverLastError();
    WcaLog(LOGMSG_STANDARD, "SetupVerifyInfFileW failed for %ws: 0x%x.", source, hr);
    return hr;
}

static HRESULT StagePackage(const DriverUndoRecord& record, DriverPackageIdentity& staged,
    std::wstring& published, bool& created)
{
    DRIVER_TRY(VerifyDriverSource(record.sourceInf.c_str()));
    wchar_t destination[MAX_PATH] = {};
    DWORD needed = 0;
    BOOL copied = SetupCopyOEMInfW(record.sourceInf.c_str(), NULL, SPOST_PATH, SP_COPY_NOOVERWRITE,
        destination, _countof(destination), &needed, NULL);
    DWORD error = copied ? ERROR_SUCCESS : GetLastError();
    if (!copied && error != ERROR_FILE_EXISTS)
    {
        WcaLog(LOGMSG_STANDARD, "SetupCopyOEMInfW failed: %lu.", error);
        return HRESULT_FROM_WIN32(error ? error : ERROR_GEN_FAILURE);
    }
    created = copied != FALSE;
    published = destination;
    if (published.empty()) return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    DRIVER_TRY(PublishedIdentity(published.c_str(), staged));
    return SameDriverPackage(record.sourcePackage, staged) ? S_OK : HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
}

static PCWSTR DriverMetadataContainers[] = {
    L"SOFTWARE\\RedHat", L"SOFTWARE\\RedHat\\Virtio-Win", L"SOFTWARE\\RedHat\\Virtio-Win\\Components" };

static HRESULT ReadMetadata(PCWSTR path, DriverMetadata& metadata)
{
    bool* present[] = { &metadata.redHatPresent, &metadata.virtioPresent, &metadata.componentsPresent };
    for (unsigned i = 0; i < _countof(present); ++i)
    {
        CRegKey container;
        LONG error = RegOpenKeyExW(HKEY_LOCAL_MACHINE, DriverMetadataContainers[i], 0, KEY_QUERY_VALUE, &container.m_hKey);
        if (error == ERROR_FILE_NOT_FOUND) continue;
        if (error != ERROR_SUCCESS) return HRESULT_FROM_WIN32(error);
        *present[i] = true;
        container.Close();
    }
    CRegKey key;
    LONG error = RegOpenKeyExW(HKEY_LOCAL_MACHINE, path, 0, KEY_QUERY_VALUE, &key.m_hKey);
    if (error == ERROR_FILE_NOT_FOUND) return S_OK;
    if (error != ERROR_SUCCESS) return HRESULT_FROM_WIN32(error);
    metadata.keyPresent = true;
    PCWSTR names[] = { L"oem", L"strongname", L"component", L"createdByComponent", L"infHash", L"catalogHash" };
    DriverMetadataValue* values[] = { &metadata.oem, &metadata.strongname,
        &metadata.component, &metadata.createdByComponent, &metadata.infHash, &metadata.catalogHash };
    for (unsigned i = 0; i < _countof(names); ++i)
    {
        DWORD size = 0, type = 0;
        error = RegQueryValueExW(key, names[i], NULL, &type, NULL, &size);
        if (error == ERROR_FILE_NOT_FOUND) { error = ERROR_SUCCESS; continue; }
        if (error != ERROR_SUCCESS) break;
        if (type != REG_SZ || size < sizeof(wchar_t) || size > 65536 || size % sizeof(wchar_t))
        { error = ERROR_INVALID_DATA; break; }
        std::vector<wchar_t> text(size / sizeof(wchar_t));
        error = RegQueryValueExW(key, names[i], NULL, &type, reinterpret_cast<BYTE*>(text.data()), &size);
        if (error != ERROR_SUCCESS) break;
        if (text.back() || wcslen(text.data()) + 1 != text.size()) { error = ERROR_INVALID_DATA; break; }
        values[i]->present = true;
        values[i]->value = text.data();
    }
    key.Close();
    return HRESULT_FROM_WIN32(error);
}

static HRESULT WriteMetadata(PCWSTR path, const DriverMetadata& metadata)
{
    CRegKey key;
    LONG error = RegCreateKeyExW(HKEY_LOCAL_MACHINE, path, 0, NULL, 0,
        KEY_SET_VALUE | KEY_QUERY_VALUE, NULL, &key.m_hKey, NULL);
    if (error != ERROR_SUCCESS) return HRESULT_FROM_WIN32(error);
    PCWSTR names[] = { L"oem", L"strongname", L"component", L"createdByComponent", L"infHash", L"catalogHash" };
    const DriverMetadataValue* values[] = { &metadata.oem, &metadata.strongname,
        &metadata.component, &metadata.createdByComponent, &metadata.infHash, &metadata.catalogHash };
    HRESULT first = S_OK;
    for (unsigned i = 0; i < _countof(names); ++i)
    {
        const auto& value = *values[i];
        error = value.present ? RegSetValueExW(key, names[i], 0, REG_SZ,
            reinterpret_cast<const BYTE*>(value.value.c_str()),
            static_cast<DWORD>((value.value.size() + 1) * sizeof(wchar_t))) : RegDeleteValueW(key, names[i]);
        if (!value.present && error == ERROR_FILE_NOT_FOUND) error = ERROR_SUCCESS;
        if (error != ERROR_SUCCESS && SUCCEEDED(first)) first = HRESULT_FROM_WIN32(error);
    }
    DWORD subkeys = 0, valueCount = 0;
    error = RegQueryInfoKeyW(key, NULL, NULL, NULL, &subkeys, NULL, NULL, &valueCount, NULL, NULL, NULL, NULL);
    if (error != ERROR_SUCCESS && SUCCEEDED(first)) first = HRESULT_FROM_WIN32(error);
    key.Close();
    if (SUCCEEDED(first) && !metadata.keyPresent && !subkeys && !valueCount)
    {
        error = RegDeleteKeyW(HKEY_LOCAL_MACHINE, path);
        if (error != ERROR_SUCCESS) first = HRESULT_FROM_WIN32(error);
    }
    if (SUCCEEDED(first) && !metadata.keyPresent)
    {
        bool existed[] = { metadata.redHatPresent, metadata.virtioPresent, metadata.componentsPresent };
        for (int i = 2; i >= 0; --i)
        {
            if (existed[i]) continue;
            CRegKey container;
            error = RegOpenKeyExW(HKEY_LOCAL_MACHINE, DriverMetadataContainers[i], 0, KEY_QUERY_VALUE, &container.m_hKey);
            if (error == ERROR_FILE_NOT_FOUND) continue;
            if (error != ERROR_SUCCESS) { first = HRESULT_FROM_WIN32(error); break; }
            DWORD children = 0, values = 0;
            error = RegQueryInfoKeyW(container, NULL, NULL, NULL, &children, NULL, NULL, &values, NULL, NULL, NULL, NULL);
            container.Close();
            if (error != ERROR_SUCCESS) { first = HRESULT_FROM_WIN32(error); break; }
            // Only empty containers absent at capture can be ours; never delete other products' data.
            if (children || values) continue;
            error = RegDeleteKeyW(HKEY_LOCAL_MACHINE, DriverMetadataContainers[i]);
            if (error != ERROR_SUCCESS) { first = HRESULT_FROM_WIN32(error); break; }
        }
    }
    return first;
}

static HRESULT ReadMetadataPackageIdentity(PCWSTR path, const DriverPackageIdentity& expected, bool& known, bool& shared)
{
    shared = true; // A failed read must never grant an unshared/removable classification.
    std::uint32_t error = CheckRetainedDriverMetadataHash(expected,
        [&](PCWSTR name, std::wstring& value) -> std::uint32_t {
            wchar_t hash[65] = {};
            DWORD size = sizeof(hash);
            LONG status = RegGetValueW(HKEY_LOCAL_MACHINE, path, name, RRF_RT_REG_SZ, NULL, hash, &size);
            if (status == ERROR_SUCCESS) value = hash;
            return status; // RegGetValue returns its error directly; GetLastError is unrelated.
        }, known, [&]() { shared = false; });
    return HRESULT_FROM_WIN32(error);
}

static HRESULT DeviceString(HDEVINFO set, SP_DEVINFO_DATA& device, const DEVPROPKEY& key,
    std::wstring& value, bool optional = false)
{
    DWORD size = 0;
    DEVPROPTYPE type;
    SetupDiGetDevicePropertyW(set, &device, &key, &type, NULL, 0, &size, 0);
    DWORD error = GetLastError();
    if (optional && error == ERROR_NOT_FOUND) { value.clear(); return S_FALSE; }
    if (error != ERROR_INSUFFICIENT_BUFFER || size < sizeof(wchar_t) || size % sizeof(wchar_t))
        return HRESULT_FROM_WIN32(error ? error : ERROR_INVALID_DATA);
    std::vector<wchar_t> buffer(size / sizeof(wchar_t));
    if (!SetupDiGetDevicePropertyW(set, &device, &key, &type,
        reinterpret_cast<BYTE*>(buffer.data()), size, &size, 0)) return DriverLastError();
    if (type != DEVPROP_TYPE_STRING || buffer.back() || wcslen(buffer.data()) + 1 != buffer.size())
        return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    value = buffer.data();
    return S_OK;
}

static HRESULT CurrentBinding(HDEVINFO set, SP_DEVINFO_DATA& device, DriverBinding& binding)
{
    wchar_t instance[MAX_DEVICE_ID_LEN];
    if (!SetupDiGetDeviceInstanceIdW(set, &device, instance, _countof(instance), NULL)) return DriverLastError();
    binding.instance = instance;
    std::wstring oem;
    HRESULT hr = DeviceString(set, device, DEVPKEY_Device_DriverInfPath, oem, true);
    DRIVER_TRY(hr);
    binding.hasDriver = hr != S_FALSE && !oem.empty();
    if (!binding.hasDriver) return S_OK;
    std::vector<wchar_t> store;
    hr = StoreLocation(oem.c_str(), store);
    if (FAILED(hr))
    {
        // Only an unresolvable published name qualifies; errors reading a resolved package fail closed below.
        // Observed x86 damaged history: a non-present VIOINPUT child still names a deleted package (oem8.---).
        // No package exists to select or restore, so it is captured, checked and inverted as NULL-driver.
        DEVPROPTYPE type;
        DEVPROP_BOOLEAN present = DEVPROP_TRUE;
        if (!SetupDiGetDevicePropertyW(set, &device, &DEVPKEY_Device_IsPresent, &type,
            reinterpret_cast<BYTE*>(&present), sizeof(present), NULL, 0) || type != DEVPROP_TYPE_BOOLEAN) return hr;
        if (!DriverStalePhantomBinding(present != DEVPROP_FALSE, hr)) return hr;
        WcaLog(LOGMSG_STANDARD, "Non-present %ws names absent package %ws (0x%x); treated as NULL-driver.", instance, oem.c_str(), hr);
        binding = DriverBinding();
        binding.instance = instance;
        return S_OK;
    }
    DRIVER_TRY(PackageIdentity(store.data(), binding.package));
    DRIVER_TRY(DeviceString(set, device, DEVPKEY_Device_DriverInfSection, binding.section));
    DRIVER_TRY(DeviceString(set, device, DEVPKEY_Device_DriverInfSectionExt, binding.sectionExt, true));
    return S_OK;
}

static bool SameBinding(const DriverBinding& a, const DriverBinding& b)
{
    return a.hasDriver == b.hasDriver && (!a.hasDriver ||
        (SameDriverPackage(a.package, b.package) && _wcsicmp(a.section.c_str(), b.section.c_str()) == 0 &&
        _wcsicmp(a.sectionExt.c_str(), b.sectionExt.c_str()) == 0));
}

static HRESULT ActualDriverSection(PCWSTR path, PCWSTR section, std::wstring& actual)
{
    HINF inf = SetupOpenInfFileW(path, NULL, INF_STYLE_WIN4, NULL);
    if (inf == INVALID_HANDLE_VALUE) return DriverLastError();
    wchar_t resolved[LINE_LEN];
    BOOL selected = SetupDiGetActualSectionToInstallW(inf, section, resolved, _countof(resolved), NULL, NULL);
    HRESULT hr = selected ? S_OK : DriverLastError();
    SetupCloseInfFile(inf);
    if (SUCCEEDED(hr)) actual = resolved;
    return hr;
}

// Restrict the compatible list to one exact signed INF; selection remains SetupAPI's platform-aware selection.
static HRESULT SelectDriver(HDEVINFO set, SP_DEVINFO_DATA& device, PCWSTR inf,
    const DriverBinding* prior, SP_DRVINFO_DATA_W& selected)
{
    SetupDiDestroyDriverInfoList(set, &device, SPDIT_COMPATDRIVER);
    SP_DEVINSTALL_PARAMS_W parameters = { sizeof(parameters) };
    if (!SetupDiGetDeviceInstallParamsW(set, &device, &parameters)) return DriverLastError();
    parameters.Flags |= DI_ENUMSINGLEINF | DI_QUIETINSTALL;
    parameters.FlagsEx &= ~DI_FLAGSEX_INSTALLEDDRIVER;
    parameters.FlagsEx |= DI_FLAGSEX_ALLOWEXCLUDEDDRVS;
    HRESULT hr = StringCchCopyW(parameters.DriverPath, _countof(parameters.DriverPath), inf);
    DRIVER_TRY(hr);
    if (!SetupDiSetDeviceInstallParamsW(set, &device, &parameters) ||
        !SetupDiBuildDriverInfoList(set, &device, SPDIT_COMPATDRIVER)) return DriverLastError();
    SP_DRVINFO_DATA_W first = { sizeof(first) };
    if (!SetupDiEnumDriverInfoW(set, &device, SPDIT_COMPATDRIVER, 0, &first))
    {
        DWORD error = GetLastError();
        return error == ERROR_NO_MORE_ITEMS ? HRESULT_FROM_WIN32(ERROR_NO_COMPAT_DRIVERS) : DriverLastError();
    }
    if (!prior)
    {
        // Matching query only: the compatible list includes hardware and compatible IDs, not forward ranking.
        selected = first;
        return S_OK;
    }
    for (DWORD index = 0;; ++index)
    {
        SP_DRVINFO_DATA_W candidate = { sizeof(candidate) };
        if (!SetupDiEnumDriverInfoW(set, &device, SPDIT_COMPATDRIVER, index, &candidate))
        {
            DWORD error = GetLastError();
            return error == ERROR_NO_MORE_ITEMS ? HRESULT_FROM_WIN32(ERROR_NO_COMPAT_DRIVERS) : DriverLastError();
        }
        DWORD needed = 0;
        SetupDiGetDriverInfoDetailW(set, &device, &candidate, NULL, 0, &needed);
        if (!needed) return DriverLastError();
        std::vector<BYTE> data(needed);
        auto detail = reinterpret_cast<PSP_DRVINFO_DETAIL_DATA_W>(data.data());
        detail->cbSize = sizeof(*detail);
        if (!SetupDiGetDriverInfoDetailW(set, &device, &candidate, detail, needed, NULL)) return DriverLastError();
        std::wstring actual;
        DRIVER_TRY(ActualDriverSection(inf, detail->SectionName, actual));
        if (DriverBindingSectionMatches(*prior, actual,
            [](PCWSTR a, PCWSTR b) { return _wcsicmp(a, b) == 0; }))
        {
            selected = candidate;
            if (!SetupDiSetSelectedDriverW(set, &device, &selected)) return DriverLastError();
            return S_OK;
        }
    }
}

// Reject before staging/removal if current platform cannot select the captured exact section.
static HRESULT VerifyPriorSelectable(HDEVINFO set, SP_DEVINFO_DATA& device, const DriverBinding& prior)
{
    if (!prior.hasDriver) return S_OK;
    SP_DRVINFO_DATA_W driver = { sizeof(driver) };
    DriverPackageIdentity current;
    std::wstring published;
    DRIVER_TRY(ResolvePublishedPackage(prior.package, published, current));
    HRESULT hr = SelectDriver(set, device, current.storeInf.c_str(), &prior, driver);
    if (FAILED(hr)) WcaLog(LOGMSG_STANDARD, "Cannot select exact prior driver section for %ws; refusing before native package mutation (0x%x).", prior.instance.c_str(), hr);
    return hr;
}

static HRESULT InstallBinding(HDEVINFO set, SP_DEVINFO_DATA& device, const DriverBinding& desired, bool& reboot)
{
    SP_DRVINFO_DATA_W selected = { sizeof(selected) };
    if (desired.hasDriver)
    {
        DriverPackageIdentity existing;
        std::wstring published;
        DRIVER_TRY(ResolvePublishedPackage(desired.package, published, existing));
        DRIVER_TRY(SelectDriver(set, device, existing.storeInf.c_str(), &desired, selected));
    }
    BOOL needed = FALSE;
    BOOL installed = DiInstallDevice(NULL, set, &device, desired.hasDriver ? &selected : NULL,
        desired.hasDriver ? 0 : DIIDFLAG_INSTALLNULLDRIVER, &needed);
    reboot = reboot || needed;
    if (!installed) return DriverLastError();
    DriverBinding actual;
    DRIVER_TRY(CurrentBinding(set, device, actual));
    if (!SameBinding(desired, actual)) return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    WcaLog(LOGMSG_STANDARD, "Verified prior binding restored: %ws package=%ws section=%ws%ws.",
        desired.instance.c_str(), desired.hasDriver ? desired.package.storeInf.c_str() : L"NULL-driver",
        desired.section.c_str(), desired.sectionExt.c_str());
    return S_OK;
}

struct DriverService
{
    SC_HANDLE handle = NULL;
    ~DriverService() { if (handle) CloseServiceHandle(handle); }
};

// present=false only for ERROR_SERVICE_DOES_NOT_EXIST; any other SCM failure is an error.
static HRESULT QueryDriverService(SC_HANDLE service, DriverServiceConfig& config)
{
    DWORD needed = 0;
    QueryServiceConfigW(service, NULL, 0, &needed);
    if (!needed) return DriverLastError();
    std::vector<BYTE> data(needed);
    auto query = reinterpret_cast<LPQUERY_SERVICE_CONFIGW>(data.data());
    if (!QueryServiceConfigW(service, query, needed, &needed)) return DriverLastError();
    config.imagePath = query->lpBinaryPathName ? query->lpBinaryPathName : L"";
    config.loadOrderGroup = query->lpLoadOrderGroup ? query->lpLoadOrderGroup : L"";
    config.type = query->dwServiceType; config.start = query->dwStartType; config.error = query->dwErrorControl;
    config.dependencies.clear();
    for (PCWSTR dependency = query->lpDependencies; dependency && *dependency; dependency += wcslen(dependency) + 1)
        config.dependencies.push_back(dependency);
    return S_OK;
}

static HRESULT OpenDriverService(PCWSTR name, DWORD access, DriverService& scm, DriverService& service, bool& present)
{
    present = false;
    scm.handle = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm.handle) return DriverLastError();
    service.handle = OpenServiceW(scm.handle, name, access);
    if (!service.handle) return GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST ? S_OK : DriverLastError();
    present = true;
    return S_OK;
}

static bool DriverInfNamePresent(const std::vector<std::wstring>& names, const std::wstring& name)
{
    return std::any_of(names.begin(), names.end(),
        [&](const std::wstring& prior) { return _wcsicmp(prior.c_str(), name.c_str()) == 0; });
}

static HRESULT ReadDriverServiceDirective(HINF inf, PCWSTR section, PCWSTR key, bool allFields,
    std::vector<std::wstring>& values)
{
    INFCONTEXT line;
    SetLastError(ERROR_SUCCESS);
    BOOL found = SetupFindFirstLineW(inf, section, key, &line);
    while (found)
    {
        DWORD fields = SetupGetFieldCount(&line);
        if (!fields) return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
        for (DWORD field = 1; field <= (allFields ? fields : 1); ++field)
        {
            wchar_t value[MAX_PATH];
            if (!SetupGetStringFieldW(&line, field, value, _countof(value), NULL)) return DriverLastError();
            if (!*value || values.size() >= 1024) return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
            values.push_back(value);
        }
        SetLastError(ERROR_SUCCESS);
        found = SetupFindNextMatchLineW(&line, key, &line);
    }
    DWORD error = GetLastError();
    if (error == ERROR_LINE_NOT_FOUND || error == ERROR_NO_MORE_ITEMS) return S_OK;
    return HRESULT_FROM_WIN32(error ? error : ERROR_GEN_FAILURE);
}

// SetupAPI searches appended inbox INFs as one handle. Needs names are explicit service-section names.
static HRESULT CollectDriverServiceSection(HINF inf, const std::wstring& section,
    std::vector<std::wstring>& included, std::vector<std::wstring>& active,
    std::vector<std::wstring>& complete, std::vector<std::wstring>& names)
{
    if (DriverInfNamePresent(active, section)) return HRESULT_FROM_WIN32(ERROR_CIRCULAR_DEPENDENCY);
    if (DriverInfNamePresent(complete, section)) return S_OK;
    if (section.empty() || section.size() >= MAX_PATH || active.size() >= 32 || active.size() + complete.size() >= 256)
        return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    if (SetupGetLineCountW(inf, section.c_str()) < 0) return HRESULT_FROM_WIN32(ERROR_SECTION_NOT_FOUND);
    active.push_back(section);
    std::vector<std::wstring> files;
    DRIVER_TRY(ReadDriverServiceDirective(inf, section.c_str(), L"Include", true, files));
    for (const auto& file : files)
    {
        // DDInstall.Services Include names system-supplied INFs; do not invent a relative-path search.
        if (file.find_first_of(L"\\/:") != std::wstring::npos) return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        if (DriverInfNamePresent(included, file)) continue;
        if (included.size() >= 64) return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
        if (!SetupOpenAppendInfFileW(file.c_str(), inf, NULL)) return DriverLastError();
        included.push_back(file);
    }
    std::vector<std::wstring> removed, needed;
    DRIVER_TRY(ReadDriverServiceDirective(inf, section.c_str(), L"DelService", false, removed));
    // Service deletion has no configuration-only inverse; refuse it before touching the device.
    if (!removed.empty()) return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
    DRIVER_TRY(ReadDriverServiceDirective(inf, section.c_str(), L"Needs", true, needed));
    for (const auto& dependency : needed)
        DRIVER_TRY(CollectDriverServiceSection(inf, dependency, included, active, complete, names));
    DRIVER_TRY(ReadDriverServiceDirective(inf, section.c_str(), L"AddService", false, names));
    active.pop_back();
    complete.push_back(section);
    return S_OK;
}

static HRESULT CaptureDriverServiceNames(PCWSTR path, const std::wstring& section,
    std::vector<std::wstring>& names)
{
    HINF inf = SetupOpenInfFileW(path, NULL, INF_STYLE_WIN4, NULL);
    if (inf == INVALID_HANDLE_VALUE) return DriverLastError();
    HRESULT hr = S_OK;
    if (SetupGetLineCountW(inf, section.c_str()) >= 0)
    {
        std::vector<std::wstring> included, active, complete;
        size_t priorIncludes;
        do
        {
            priorIncludes = included.size();
            active.clear(); complete.clear(); names.clear();
            hr = CollectDriverServiceSection(inf, section, included, active, complete, names);
            // New appended files can add directives to an earlier section; capture only the stable view.
        } while (SUCCEEDED(hr) && included.size() != priorIncludes);
    }
    else if (GetLastError() != ERROR_SECTION_NOT_FOUND) hr = DriverLastError();
    SetupCloseInfFile(inf);
    return hr;
}

// Existing services named by AddService in any install section this INF offers the device, including
// services reached through Include/Needs. New service creation has no safe deletion inverse (limit).
static HRESULT CaptureTargetServices(HDEVINFO set, SP_DEVINFO_DATA& device, DriverUndoRecord& record)
{
    for (DWORD index = 0;; ++index)
    {
        SP_DRVINFO_DATA_W candidate = { sizeof(candidate) };
        if (!SetupDiEnumDriverInfoW(set, &device, SPDIT_COMPATDRIVER, index, &candidate))
            return GetLastError() == ERROR_NO_MORE_ITEMS ? S_OK : DriverLastError();
        DWORD needed = 0;
        SetupDiGetDriverInfoDetailW(set, &device, &candidate, NULL, 0, &needed);
        if (!needed) return DriverLastError();
        std::vector<BYTE> data(needed);
        auto detail = reinterpret_cast<PSP_DRVINFO_DETAIL_DATA_W>(data.data());
        detail->cbSize = sizeof(*detail);
        if (!SetupDiGetDriverInfoDetailW(set, &device, &candidate, detail, needed, NULL)) return DriverLastError();
        std::wstring actual;
        DRIVER_TRY(ActualDriverSection(record.sourceInf.c_str(), detail->SectionName, actual));
        std::vector<std::wstring> names;
        DRIVER_TRY(CaptureDriverServiceNames(record.sourceInf.c_str(), actual + L".Services", names));
        for (const auto& name : names)
        {
            if (std::any_of(record.services.begin(), record.services.end(),
                [&](const DriverServiceConfig& s) { return _wcsicmp(s.name.c_str(), name.c_str()) == 0; })) continue;
            DriverService scm, service;
            bool present;
            DRIVER_TRY(OpenDriverService(name.c_str(), SERVICE_QUERY_CONFIG, scm, service, present));
            if (!present) continue;
            DriverServiceConfig config;
            config.name = name;
            DRIVER_TRY(QueryDriverService(service.handle, config));
            if (config.imagePath.empty()) return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
            record.services.push_back(config);
        }
    }
}

static bool SameDriverService(const DriverServiceConfig& a, const DriverServiceConfig& b)
{
    return _wcsicmp(a.imagePath.c_str(), b.imagePath.c_str()) == 0 && _wcsicmp(a.loadOrderGroup.c_str(), b.loadOrderGroup.c_str()) == 0 &&
        a.type == b.type && a.start == b.start && a.error == b.error &&
        a.dependencies.size() == b.dependencies.size() &&
        std::equal(a.dependencies.begin(), a.dependencies.end(), b.dependencies.begin(),
            [](const std::wstring& x, const std::wstring& y) { return _wcsicmp(x.c_str(), y.c_str()) == 0; });
}

// Immediately before the package-wide call: captured services must still be as captured, else refuse.
static HRESULT VerifyDriverServicesUnchanged(const DriverUndoRecord& record)
{
    for (const auto& prior : record.services)
    {
        DriverService scm, service;
        bool present;
        DRIVER_TRY(OpenDriverService(prior.name.c_str(), SERVICE_QUERY_CONFIG, scm, service, present));
        DriverServiceConfig actual;
        if (present) DRIVER_TRY(QueryDriverService(service.handle, actual));
        actual.name = prior.name;
        if (!present || !SameDriverService(actual, prior)) return HRESULT_FROM_WIN32(ERROR_INVALID_STATE);
    }
    return S_OK;
}

// Supported SCM inverse of the AddService side effect; verified by re-query, never a raw registry write.
static HRESULT RestoreDriverService(const DriverServiceConfig& prior)
{
    DriverService scm, service;
    bool present;
    DRIVER_TRY(OpenDriverService(prior.name.c_str(), SERVICE_QUERY_CONFIG | SERVICE_CHANGE_CONFIG, scm, service, present));
    if (!present) return HRESULT_FROM_WIN32(ERROR_SERVICE_DOES_NOT_EXIST);
    DriverServiceConfig actual;
    DRIVER_TRY(QueryDriverService(service.handle, actual));
    auto same = [&]() { return SameDriverService(actual, prior); };
    if (same()) return S_OK;
    std::vector<WCHAR> dependencies;
    for (const auto& dependency : prior.dependencies)
    {
        dependencies.insert(dependencies.end(), dependency.begin(), dependency.end());
        dependencies.push_back(L'\0');
    }
    if (dependencies.empty()) dependencies.push_back(L'\0');
    dependencies.push_back(L'\0');
    // A non-null empty MULTI_SZ clears dependencies; NULL would leave changed dependencies in place.
    if (!ChangeServiceConfigW(service.handle, prior.type, prior.start, prior.error, prior.imagePath.c_str(),
        prior.loadOrderGroup.c_str(), NULL, dependencies.data(), NULL, NULL, NULL)) return DriverLastError();
    DRIVER_TRY(QueryDriverService(service.handle, actual));
    if (!same()) return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    WcaLog(LOGMSG_STANDARD, "Verified prior service configuration restored: %ws image=%ws.", prior.name.c_str(), prior.imagePath.c_str());
    return S_OK;
}

static HRESULT CaptureBindings(DriverUndoRecord& record, bool installing)
{
    // Installing also inspects non-present devnodes: binding a parent can revive a phantom child that then keeps the new package in use.
    DriverDevices set(SetupDiGetClassDevsW(NULL, NULL, NULL, DIGCF_ALLCLASSES));
    if (set == INVALID_HANDLE_VALUE) return DriverLastError();
    std::vector<std::wstring> present;
    if (installing)
    {
        DriverDevices live(SetupDiGetClassDevsW(NULL, NULL, NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT));
        if (live == INVALID_HANDLE_VALUE) return DriverLastError();
        for (DWORD i = 0;; ++i)
        {
            SP_DEVINFO_DATA device = { sizeof(device) };
            if (!SetupDiEnumDeviceInfo(live, i, &device))
            {
                if (GetLastError() != ERROR_NO_MORE_ITEMS) return DriverLastError();
                break;
            }
            wchar_t instance[MAX_DEVICE_ID_LEN];
            if (!SetupDiGetDeviceInstanceIdW(live, &device, instance, _countof(instance), NULL)) return DriverLastError();
            present.push_back(instance);
        }
    }
    auto same = [](PCWSTR a, PCWSTR b) { return _wcsicmp(a, b) == 0; };
    // Pass 1: matching devnodes with presence and parent only; no binding reads yet.
    std::vector<DWORD> indexes;
    std::vector<DriverScopeNode> nodes;
    for (DWORD i = 0;; ++i)
    {
        SP_DEVINFO_DATA device = { sizeof(device) };
        if (!SetupDiEnumDeviceInfo(set, i, &device))
        {
            if (GetLastError() != ERROR_NO_MORE_ITEMS) return DriverLastError();
            break;
        }
        HRESULT hr;
        if (installing)
        {
            SP_DRVINFO_DATA_W target = { sizeof(target) };
            hr = SelectDriver(set, device, record.sourceInf.c_str(), NULL, target);
            if (hr == HRESULT_FROM_WIN32(ERROR_NO_COMPAT_DRIVERS)) continue;
            DRIVER_TRY(hr);
        }
        else
        {
            std::wstring oem;
            hr = DeviceString(set, device, DEVPKEY_Device_DriverInfPath, oem, true);
            DRIVER_TRY(hr);
            if (hr == S_FALSE || _wcsicmp(oem.c_str(), record.publishedInf.substr(record.publishedInf.find_last_of(L"\\/") + 1).c_str()) != 0)
                continue;
        }
        DriverScopeNode node;
        wchar_t instance[MAX_DEVICE_ID_LEN];
        if (!SetupDiGetDeviceInstanceIdW(set, &device, instance, _countof(instance), NULL)) return DriverLastError();
        node.binding.instance = instance;
        node.present = !installing || std::any_of(present.begin(), present.end(),
            [&](const std::wstring& id) { return same(id.c_str(), instance); });
        DRIVER_TRY(DeviceString(set, device, DEVPKEY_Device_Parent, node.parent, true));
        nodes.push_back(node);
        indexes.push_back(i);
    }
    // Pass 2: bindings only for present matches and phantom children of present matches; unrelated
    // phantoms (possibly with stale packages) are neither read nor mutated.
    std::vector<DriverScopeNode> kept;
    for (size_t n = 0; n < nodes.size(); ++n)
    {
        DriverScopeNode node = nodes[n];
        if (!node.present && std::none_of(nodes.begin(), nodes.end(), [&](const DriverScopeNode& p) {
                return p.present && same(p.binding.instance.c_str(), node.parent.c_str()); }))
            continue;
        SP_DEVINFO_DATA device = { sizeof(device) };
        if (!SetupDiEnumDeviceInfo(set, indexes[n], &device)) return DriverLastError();
        DRIVER_TRY(CurrentBinding(set, device, node.binding));
        if (installing || (node.binding.hasDriver && SameDriverPackage(node.binding.package, record.sourcePackage)))
        {
            // Only kept nodes can be reinstalled by the inverse. Rebuild the source list before it is
            // replaced by VerifyPriorSelectable, including owned-package uninstall snapshots.
            SP_DRVINFO_DATA_W target = { sizeof(target) };
            DRIVER_TRY(SelectDriver(set, device, record.sourceInf.c_str(), NULL, target));
            DRIVER_TRY(CaptureTargetServices(set, device, record));
            DRIVER_TRY(VerifyPriorSelectable(set, device, node.binding));
            kept.push_back(node);
        }
    }
    set.Close();
    std::vector<DriverBinding> scope;
    // Parent-first order; the inverse replays in reverse. A parent cycle refuses before any mutation.
    if (!BuildDriverBindingScope(kept, same, scope)) return HRESULT_FROM_WIN32(ERROR_INVALID_STATE);
    record.bindings.insert(record.bindings.end(), scope.begin(), scope.end());
    return S_OK;
}

struct DriverScript
{
    WCA_CASCRIPT_HANDLE handle = NULL;
    ~DriverScript() { WcaCaScriptClose(handle, WCA_CASCRIPT_CLOSE_PRESERVE); }
};

static HRESULT ScriptField(LPWSTR* cursor, std::wstring& value)
{
    if (!*cursor || !**cursor) return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    LPWSTR text = NULL;
    HRESULT hr = WcaReadStringFromCaData(cursor, &text);
    if (SUCCEEDED(hr)) value = text;
    ReleaseStr(text);
    return hr;
}

static HRESULT AppendDriverEvent(WCA_CASCRIPT_HANDLE script, const std::vector<std::wstring>& event)
{
    if (event.empty() || event.size() > 1048576) return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    for (const auto& field : event)
        if (field.size() > 32767 || field.find(static_cast<wchar_t>(128)) != std::wstring::npos ||
            field.find(L'\0') != std::wstring::npos) return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    DRIVER_TRY(WcaCaScriptWriteNumber(script, static_cast<DWORD>(event.size())));
    for (const auto& field : event) DRIVER_TRY(WcaCaScriptWriteString(script, field.c_str()));
    DRIVER_TRY(WcaCaScriptWriteString(script, L"complete"));
    WcaCaScriptFlush(script); // WiX best effort only, not a power-loss durability guarantee.
    return S_OK;
}

static HRESULT ReadDriverPlan(PCWSTR data, DriverPlan& plan)
{
    LPWSTR copy = NULL;
    HRESULT hr = StrAllocString(&copy, data, 0);
    if (FAILED(hr)) return hr;
    LPWSTR cursor = copy;
    std::wstring mode, count;
    hr = ScriptField(&cursor, plan.key);
    if (SUCCEEDED(hr)) hr = ScriptField(&cursor, mode);
    if (SUCCEEDED(hr)) hr = ScriptField(&cursor, count);
    unsigned total = 0;
    GUID guid;
    if (SUCCEEDED(hr) && (plan.key.find(L"DrvTx-") != 0 || plan.key.size() != 44 ||
        FAILED(CLSIDFromString(plan.key.c_str() + 6, &guid)) ||
        (mode != L"0" && mode != L"1") || !DriverStateNumber(count, total) || total > 65535))
        hr = HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    plan.installing = mode == L"1";
    for (unsigned i = 0; SUCCEEDED(hr) && i < total; ++i)
    {
        DriverPlanEntry entry;
        std::wstring flags, sole;
        hr = ScriptField(&cursor, entry.source);
        if (SUCCEEDED(hr)) hr = ScriptField(&cursor, entry.component);
        if (SUCCEEDED(hr)) hr = ScriptField(&cursor, flags);
        if (SUCCEEDED(hr)) hr = ScriptField(&cursor, sole);
        if (SUCCEEDED(hr) && (entry.component.size() != 38 || FAILED(CLSIDFromString(entry.component.c_str(), &guid)) ||
            !DriverStateNumber(flags, entry.flags) || (sole != L"0" && sole != L"1")))
            hr = HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
        entry.soleOwner = sole == L"1";
        if (SUCCEEDED(hr)) plan.entries.push_back(entry);
    }
    if (SUCCEEDED(hr) && cursor && *cursor) hr = HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    ReleaseStr(copy);
    if (SUCCEEDED(hr))
    {
        LPWSTR product = NULL;
        hr = WcaGetProperty(L"ProductCode", &product); // Supported in deferred and rollback, unlike commit.
        if (SUCCEEDED(hr)) plan.product = product;
        ReleaseStr(product);
        if (SUCCEEDED(hr) && FAILED(CLSIDFromString(plan.product.c_str(), &guid))) hr = HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    }
    return hr;
}

static HRESULT LoadDriverRecords(const DriverPlan& plan, std::vector<DriverUndoRecord>& records, bool rollback = false)
{
    DriverScript script;
    DRIVER_TRY(WcaCaScriptOpen(WCA_ACTION_INSTALL, WCA_CASCRIPT_ROLLBACK, FALSE, plan.key.c_str(), &script.handle));
    std::wstring text;
    HRESULT first = S_OK;
    if (!rollback)
    {
        LPWSTR data = NULL;
        DRIVER_TRY(WcaCaScriptReadAsCustomActionData(script.handle, &data));
        LARGE_INTEGER size;
        if (!GetFileSizeEx(script.handle->hScriptFile, &size)) first = DriverLastError();
        else if (size.HighPart || size.LowPart % sizeof(WCHAR)) first = HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
        else text.assign(data, size.LowPart / sizeof(WCHAR));
        ReleaseStr(data);
        DRIVER_TRY(first);
    }
    else
    {
        // Read only the handle opened by WiX for this product/key; never reopen or alter another path.
        LARGE_INTEGER size, start = {};
        if (!GetFileSizeEx(script.handle->hScriptFile, &size)) return DriverLastError();
        if (size.HighPart) return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
        if (!SetFilePointerEx(script.handle->hScriptFile, start, NULL, FILE_BEGIN)) return DriverLastError();
        HRESULT readError = HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);
        auto prefix = ReadDriverScriptPrefix(size.LowPart, [&](unsigned char* buffer, unsigned wanted, unsigned& got) {
            DWORD count = 0;
            BOOL ok = ReadFile(script.handle->hScriptFile, buffer, wanted, &count, NULL);
            got = count;
            if (!ok) readError = DriverLastError();
            else if (!got) readError = HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);
            return ok != FALSE;
        });
        text = std::move(prefix.text);
        if (prefix.status == DriverPrefixStatus::corrupt) first = HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
        else if (prefix.status == DriverPrefixStatus::readError) first = readError;
    }
    bool parsed = ParseDriverJournal(plan, text, records);
    return FAILED(first) ? first : (parsed ? S_OK : HRESULT_FROM_WIN32(ERROR_INVALID_DATA));
}

static void DeleteDriverScript(const DriverPlan& plan)
{
    DriverScript script;
    HRESULT hr = WcaCaScriptOpen(WCA_ACTION_INSTALL, WCA_CASCRIPT_ROLLBACK, FALSE, plan.key.c_str(), &script.handle);
    if (FAILED(hr))
    {
        WcaLog(LOGMSG_STANDARD, "Driver transaction housekeeping could not open own script: 0x%x.", hr);
        return;
    }
    std::wstring path = script.handle->pwzScriptPath;
    WcaCaScriptClose(script.handle, WCA_CASCRIPT_CLOSE_PRESERVE);
    script.handle = NULL;
    if (!DeleteFileW(path.c_str()))
        WcaLog(LOGMSG_STANDARD, "Driver transaction housekeeping could not delete own script: %lu.", GetLastError());
}

HRESULT BeginDriverTransaction(PCWSTR data)
{
    DriverPlan plan;
    DRIVER_TRY(ReadDriverPlan(data, plan));
    DriverScript script;
    WcaLog(LOGMSG_STANDARD, "Begin driver transaction %ws.", plan.key.c_str());
    // OPEN_ALWAYS plus checked emptiness never truncates a prior failed transaction, even on a key collision;
    // no separate open probe, whose expected not-found would be logged by wcautil as an Error.
    DRIVER_TRY(WcaCaScriptCreate(WCA_ACTION_INSTALL, WCA_CASCRIPT_ROLLBACK, FALSE, plan.key.c_str(), TRUE, &script.handle));
    LARGE_INTEGER size;
    if (!GetFileSizeEx(script.handle->hScriptFile, &size)) return DriverLastError();
    if (size.QuadPart != 0) return HRESULT_FROM_WIN32(ERROR_FILE_EXISTS);
    return AppendDriverEvent(script.handle, { DriverJournalSchema, plan.key,
        plan.installing ? L"1" : L"0", std::to_wstring(plan.entries.size()) });
}

static HRESULT RemovePublishedPackage(PCWSTR published)
{
    std::wstring name(published);
    name = name.substr(name.find_last_of(L"\\/") + 1);
    return SetupUninstallOEMInfW(name.c_str(), 0, NULL) ? S_OK : DriverLastError();
}

static bool CreatedForComponent(const DriverUndoRecord& record)
{
    return KnownDriverPackageIdentity(MetadataPackageIdentity(record.metadata)) &&
        record.metadata.component.present && record.metadata.createdByComponent.present &&
        _wcsicmp(record.metadata.component.value.c_str(), record.component.c_str()) == 0 &&
        _wcsicmp(record.metadata.createdByComponent.value.c_str(), record.component.c_str()) == 0;
}

static HRESULT OtherTypedPackageClient(const DriverPlan& plan, const DriverUndoRecord& record, bool& shared)
{
    shared = false;
    CRegKey root;
    LONG error = RegOpenKeyExW(HKEY_LOCAL_MACHINE, DriverMetadataContainers[2], 0, KEY_ENUMERATE_SUB_KEYS, &root.m_hKey);
    if (error == ERROR_FILE_NOT_FOUND) return S_OK;
    if (error != ERROR_SUCCESS) return HRESULT_FROM_WIN32(error);
    HRESULT hr = S_OK;
    for (DWORD index = 0;; ++index)
    {
        wchar_t name[256];
        DWORD size = _countof(name);
        error = RegEnumKeyExW(root, index, name, &size, NULL, NULL, NULL, NULL);
        if (error == ERROR_NO_MORE_ITEMS) break;
        if (error != ERROR_SUCCESS) { hr = HRESULT_FROM_WIN32(error); break; }
        GUID guid;
        if (size != 38 || FAILED(CLSIDFromString(name, &guid)) || _wcsicmp(name, record.component.c_str()) == 0) continue;
        bool installed = false;
        for (DWORD clientIndex = 0;; ++clientIndex)
        {
            wchar_t client[39], sid[256];
            DWORD sidSize = _countof(sid);
            MSIINSTALLCONTEXT context;
            UINT er = MsiEnumClientsExW(name, NULL, MSIINSTALLCONTEXT_MACHINE, clientIndex, client, &context, sid, &sidSize);
            if (er == ERROR_NO_MORE_ITEMS || er == ERROR_UNKNOWN_COMPONENT) break;
            if (er != ERROR_SUCCESS) { hr = HRESULT_FROM_WIN32(er); break; }
            // MSI's null-GUID system client retains Permanent components without a product registration.
            if (_wcsicmp(client, L"{00000000-0000-0000-0000-000000000000}") == 0)
            { installed = true; continue; }
            INSTALLSTATE state;
            er = MsiQueryComponentStateW(client, NULL, MSIINSTALLCONTEXT_MACHINE, name, &state);
            if (er == ERROR_UNKNOWN_COMPONENT) continue;
            if (er != ERROR_SUCCESS) { hr = HRESULT_FROM_WIN32(er); break; }
            if (state == INSTALLSTATE_LOCAL || state == INSTALLSTATE_SOURCE)
            {
                bool retiringHere = false;
                if (!plan.installing && _wcsicmp(client, plan.product.c_str()) == 0)
                    for (const auto& entry : plan.entries)
                        if (entry.soleOwner && _wcsicmp(entry.component.c_str(), name) == 0) retiringHere = true;
                if (!retiringHere) installed = true;
            }
        }
        if (FAILED(hr)) break;
        if (!installed) continue;
        bool known = false;
        hr = ReadMetadataPackageIdentity(DriverMetadataKey(name).c_str(), record.sourcePackage, known, shared);
        if (FAILED(hr)) break;
        // Known hashes are compared without consuming any Strong/OEM hints; missing hashes remain UNKNOWN.
        if (shared) break;
    }
    root.Close();
    return hr;
}

static HRESULT CaptureDriverRecord(const DriverPlanEntry& entry, const DriverPlan& plan, DriverUndoRecord& record)
{
    bool installing = plan.installing;
    record.sourceInf = entry.source; record.component = entry.component;
    record.metadataKey = DriverMetadataKey(entry.component);
    DRIVER_TRY(PackageIdentity(entry.source.c_str(), record.sourcePackage));
    DRIVER_TRY(ReadMetadata(record.metadataKey.c_str(), record.metadata));
    if (record.metadata.component.present && _wcsicmp(record.metadata.component.value.c_str(), entry.component.c_str()) != 0)
        return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    if (!installing && entry.soleOwner && CreatedForComponent(record))
    {
        DriverPackageIdentity actual;
        DRIVER_TRY(ResolvePublishedPackage(MetadataPackageIdentity(record.metadata), record.publishedInf, actual,
            record.metadata.oem.present ? record.metadata.oem.value.c_str() : L""));
        if (!SameDriverPackage(actual, record.sourcePackage)) return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
        DRIVER_TRY(VerifyDriverSource(entry.source.c_str()));
        record.sourcePackage.storeInf = actual.storeInf;
        bool shared;
        DRIVER_TRY(OtherTypedPackageClient(plan, record, shared));
        record.removeOwnedPackage = !shared;
    }
    if (installing || record.removeOwnedPackage) return CaptureBindings(record, installing);
    return S_OK;
}

static HRESULT ForwardInstall(DriverUndoRecord& record, unsigned index, WCA_CASCRIPT_HANDLE script, bool& reboot)
{
    DRIVER_TRY(StageDriverPackageWithIntent(record, index,
        [&](DriverPackageIdentity& staged, std::wstring& published, bool& created) -> std::int32_t {
            return StagePackage(record, staged, published, created);
        }, [&](const std::vector<std::wstring>& event) -> std::int32_t {
            return AppendDriverEvent(script, event);
        }, [&](const std::wstring& published) {
            HRESULT cleanup = RemovePublishedPackage(published.c_str());
            if (FAILED(cleanup)) WcaLog(LOGMSG_STANDARD, "Failed to compensate newly staged package; stage intent remains recorded: %ws status=0x%x.", published.c_str(), cleanup);
        }));
    DriverPackageIdentity staged = record.stagedPackage;
    std::wstring published = record.publishedInf;
    bool created = record.packageCreated;
    WcaLog(LOGMSG_STANDARD, "Driver stage: component=%ws published=%ws createdByThisCall=%d.",
        record.component.c_str(), published.c_str(), created);
    // DiInstallDriver acts package-wide on present matching devices. Recheck the whole captured scope first.
    DriverUndoRecord currentScope;
    currentScope.sourceInf = record.sourceInf;
    currentScope.sourcePackage = staged;
    DRIVER_TRY(CaptureBindings(currentScope, true));
    if (currentScope.bindings.size() != record.bindings.size()) return HRESULT_FROM_WIN32(ERROR_INVALID_STATE);
    for (const auto& before : record.bindings)
    {
        auto current = std::find_if(currentScope.bindings.begin(), currentScope.bindings.end(),
            [&](const DriverBinding& candidate) { return candidate.instance == before.instance; });
        if (current == currentScope.bindings.end() || !SameBinding(*current, before))
            return HRESULT_FROM_WIN32(ERROR_INVALID_STATE);
    }
    DRIVER_TRY(VerifyDriverServicesUnchanged(record));
    // Every potential binding effect must have an intent before the single package-wide API call.
    for (unsigned i = 0; i < record.bindings.size(); ++i)
        DRIVER_TRY(AppendDriverEvent(script, { L"bind", std::to_wstring(index), std::to_wstring(i) }));
    BOOL needed = FALSE;
    BOOL installed = DiInstallDriverW(NULL, record.sourceInf.c_str(), 0, &needed);
    DWORD error = installed ? ERROR_SUCCESS : GetLastError();
    reboot = reboot || needed;
    static_assert(DriverNoDeviceUpdated == ERROR_NO_MORE_ITEMS, "DiInstallDriverW no-device-updated status");
    if (!DriverPackageInstallAccepted(installed != FALSE, error)) return HRESULT_FROM_WIN32(error ? error : ERROR_GEN_FAILURE);
    if (!installed) WcaLog(LOGMSG_STANDARD, "Driver package already current for all present devices: %ws; verifying store identity and bindings.", record.sourceInf.c_str());
    // Its internal preinstallation may change OEM aliases; creation ownership remains the initial copy result.
    DRIVER_TRY(ResolvePublishedPackage(record.stagedPackage, published, staged));
    currentScope.bindings.clear();
    DRIVER_TRY(CaptureBindings(currentScope, true));
    if (currentScope.bindings.size() != record.bindings.size()) return HRESULT_FROM_WIN32(ERROR_INVALID_STATE);
    for (const auto& actual : currentScope.bindings)
    {
        auto before = std::find_if(record.bindings.begin(), record.bindings.end(),
            [&](const DriverBinding& candidate) { return candidate.instance == actual.instance; });
        if (before == record.bindings.end()) return HRESULT_FROM_WIN32(ERROR_INVALID_STATE);
        // Retaining an already better driver is valid; only changed bindings must belong to this package.
        if (!SameBinding(actual, *before) && (!actual.hasDriver || !SameDriverPackage(actual.package, staged)))
            return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    }
    DRIVER_TRY(AppendDriverEvent(script, { L"metadata", std::to_wstring(index) }));
    DriverMetadata next;
    next.keyPresent = true;
    next.oem = { true, published.substr(published.find_last_of(L"\\/") + 1) };
    next.strongname = { true, staged.storeInf };
    next.component = { true, record.component };
    next.infHash = { true, staged.infHash };
    next.catalogHash = { true, staged.catalogHash };
    bool owned = created;
    if (!owned && CreatedForComponent(record)) owned = SameDriverPackage(MetadataPackageIdentity(record.metadata), staged);
    if (owned) next.createdByComponent = { true, record.component };
    return WriteMetadata(record.metadataKey.c_str(), next);
}

static HRESULT ForwardUninstall(DriverUndoRecord& record, unsigned index, WCA_CASCRIPT_HANDLE script, bool& reboot)
{
    WcaLog(LOGMSG_STANDARD, "Driver uninstall: component=%ws knownCreatedPackage=%d.", record.component.c_str(), record.removeOwnedPackage);
    if (record.removeOwnedPackage)
    {
        DriverPackageIdentity current;
        std::wstring published;
        DRIVER_TRY(ResolvePublishedPackage(record.sourcePackage, published, current,
            record.metadata.oem.present ? record.metadata.oem.value.c_str() : L""));
        DRIVER_TRY(AppendDriverEvent(script, { L"remove", std::to_wstring(index) }));
        HMODULE newdev = LoadLibraryW(L"newdev.dll");
        if (!newdev) return DriverLastError();
        using Uninstall = BOOL (WINAPI*)(HWND, LPCWSTR, DWORD, PBOOL);
        auto uninstall = reinterpret_cast<Uninstall>(GetProcAddress(newdev, "DiUninstallDriverW"));
        BOOL needed = FALSE;
        HRESULT hr = S_OK;
        if (uninstall)
        {
            if (!uninstall(NULL, current.storeInf.c_str(), 0, &needed)) hr = DriverLastError();
            reboot = reboot || needed;
        }
        else hr = RemovePublishedPackage(published.c_str());
        FreeLibrary(newdev);
        DRIVER_TRY(hr);
    }
    // An unrelated component's typed metadata is not ours to delete.
    if (!record.metadata.keyPresent) return S_OK;
    if (record.metadata.component.present && _wcsicmp(record.metadata.component.value.c_str(), record.component.c_str()) != 0)
        return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    DRIVER_TRY(AppendDriverEvent(script, { L"metadata", std::to_wstring(index) }));
    DriverMetadata removed;
    removed.redHatPresent = record.metadata.redHatPresent;
    removed.virtioPresent = record.metadata.virtioPresent;
    removed.componentsPresent = record.metadata.componentsPresent;
    return WriteMetadata(record.metadataKey.c_str(), removed);
}

HRESULT ExecuteDriverTransaction(PCWSTR data, bool& reboot)
{
    SYSTEM_INFO native = {};
    GetNativeSystemInfo(&native);
    if ((sizeof(void*) == 8 && native.wProcessorArchitecture != PROCESSOR_ARCHITECTURE_AMD64) ||
        (sizeof(void*) == 4 && native.wProcessorArchitecture != PROCESSOR_ARCHITECTURE_INTEL))
        return HRESULT_FROM_WIN32(ERROR_IN_WOW64);
    DriverPlan plan;
    DRIVER_TRY(ReadDriverPlan(data, plan));
    std::vector<DriverUndoRecord> existing;
    DRIVER_TRY(LoadDriverRecords(plan, existing));
    if (!existing.empty()) return HRESULT_FROM_WIN32(ERROR_INVALID_STATE);
    DriverScript script;
    DRIVER_TRY(WcaCaScriptCreate(WCA_ACTION_INSTALL, WCA_CASCRIPT_ROLLBACK, FALSE, plan.key.c_str(), TRUE, &script.handle));
    for (unsigned i = 0; i < plan.entries.size(); ++i)
    {
        const auto& entry = plan.entries[i];
        DriverUndoRecord record;
        // Shared MSI components perform no package or metadata removal.
        if (!plan.installing && !entry.soleOwner) return HRESULT_FROM_WIN32(ERROR_INVALID_STATE);
        DRIVER_TRY(CaptureDriverRecord(entry, plan, record));
        std::vector<std::wstring> snapshot;
        EncodeDriverSnapshot(record, snapshot);
        snapshot.insert(snapshot.begin(), L"snapshot");
        DRIVER_TRY(AppendDriverEvent(script.handle, snapshot));
        HRESULT hr = plan.installing ? ForwardInstall(record, i, script.handle, reboot) :
            ForwardUninstall(record, i, script.handle, reboot);
        if (plan.installing && (entry.flags & REBOOT_AFTER_INSTALL)) reboot = true;
        if (!plan.installing && (entry.flags & REBOOT_AFTER_UNINSTALL)) reboot = true;
        DRIVER_TRY(hr);
        DRIVER_TRY(WcaProgressMessage(200000, FALSE));
    }
    return S_OK;
}

static HRESULT RestoreDriverRecord(DriverUndoRecord& record, bool installing, bool& reboot)
{
    if (installing && DriverStageIntentIncomplete(record))
    {
        // An intent cannot prove creation ownership. Never guess a package to delete from its hash.
        WcaLog(LOGMSG_STANDARD, "Incomplete driver stage intent: component=%ws source=%ws infHash=%ws catalogHash=%ws. Copy completion and creation ownership are unavailable; retaining own recovery journal without guessing a package to delete.",
            record.component.c_str(), record.sourceInf.c_str(), record.sourcePackage.infHash.c_str(), record.sourcePackage.catalogHash.c_str());
        return HRESULT_FROM_WIN32(ERROR_INVALID_STATE);
    }
    if ((installing && !record.stageKnown) || (!installing && !record.removalAttempted && !record.metadataAttempted)) return S_OK;
    DriverPackageIdentity restoredPackage;
    std::wstring restoredPublished;
    if (!installing && record.removalAttempted)
    {
        // MSI's reverse script has restored signed source files before this inverse executes.
        DriverPackageIdentity source;
        DRIVER_TRY(PackageIdentity(record.sourceInf.c_str(), source));
        if (!SameDriverPackage(source, record.sourcePackage)) return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
        bool created = false;
        DRIVER_TRY(StagePackage(record, restoredPackage, restoredPublished, created));
    }
    if (installing && record.stageKnown && !record.packageCreated)
    {
        HRESULT hr = ResolvePublishedPackage(record.stagedPackage, restoredPublished, restoredPackage);
        if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) || hr == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND))
        {
            bool created = false;
            hr = StagePackage(record, restoredPackage, restoredPublished, created);
        }
        DRIVER_TRY(hr);
    }
    DriverDevices set(SetupDiCreateDeviceInfoList(NULL, NULL));
    if (set == INVALID_HANDLE_VALUE) return DriverLastError();
    HRESULT first = S_OK;
    for (auto it = record.bindings.rbegin(); it != record.bindings.rend(); ++it)
    {
        if (installing ? !it->attempted : !record.removalAttempted) continue;
        DriverBinding desired = *it;
        if ((!installing && record.removalAttempted && SameDriverPackage(desired.package, record.sourcePackage)) ||
            (installing && !record.packageCreated && SameDriverPackage(desired.package, record.stagedPackage)))
            desired.package = restoredPackage;
        SP_DEVINFO_DATA device = { sizeof(device) };
        HRESULT hr = S_OK;
        if (!SetupDiOpenDeviceInfoW(set, desired.instance.c_str(), NULL, 0, &device)) hr = DriverLastError();
        DriverBinding actual;
        if (SUCCEEDED(hr)) hr = CurrentBinding(set, device, actual);
        if (SUCCEEDED(hr) && !SameBinding(actual, desired))
            hr = InstallBinding(set, device, desired, reboot);
        if (FAILED(hr))
        {
            WcaLog(LOGMSG_STANDARD, "Exact prior binding restore failed for %ws: 0x%x.", desired.instance.c_str(), hr);
            if (SUCCEEDED(first)) first = hr;
        }
    }
    set.Close();
    // Binding restoration can rewrite services. Restore the snapshot after those calls, but only
    // after a package-wide install attempt or an actual owned-package removal attempt.
    if ((installing && std::any_of(record.bindings.begin(), record.bindings.end(),
             [](const DriverBinding& b) { return b.attempted; })) || (!installing && record.removalAttempted))
        for (const auto& service : record.services)
        {
            HRESULT hr = RestoreDriverService(service);
            if (FAILED(hr))
            {
                WcaLog(LOGMSG_STANDARD, "Prior service configuration restore failed for %ws: 0x%x.", service.name.c_str(), hr);
                if (SUCCEEDED(first)) first = hr;
            }
        }
    if (record.metadataAttempted || (!installing && record.removalAttempted) ||
        (installing && !record.packageCreated && SameDriverPackage(MetadataPackageIdentity(record.metadata), record.stagedPackage)))
    {
        DriverMetadata prior = record.metadata;
        if ((!installing && record.removalAttempted) || (installing && !record.packageCreated &&
            SameDriverPackage(MetadataPackageIdentity(prior), record.stagedPackage)))
        {
            if (prior.oem.present) prior.oem.value = restoredPublished.substr(restoredPublished.find_last_of(L"\\/") + 1);
            if (prior.strongname.present) prior.strongname.value = restoredPackage.storeInf;
        }
        HRESULT hr = WriteMetadata(record.metadataKey.c_str(), prior);
        if (FAILED(hr) && SUCCEEDED(first)) first = hr;
    }
    if (installing && record.stageKnown && record.packageCreated && SUCCEEDED(first))
    {
        // Non-forced package removal cannot delete hardware nodes or rebind unexpected new users.
        DriverPackageIdentity current;
        std::wstring published;
        HRESULT hr = ResolvePublishedPackage(record.stagedPackage, published, current);
        if (SUCCEEDED(hr)) hr = RemovePublishedPackage(published.c_str());
        else if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) || hr == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND))
            hr = S_OK; // The package-wide API already removed this transaction-created package.
        WcaLog(LOGMSG_STANDARD, "Created-package inverse: %ws status=0x%x.", record.stagedPackage.storeInf.c_str(), hr);
        if (FAILED(hr)) first = hr;
    }
    return first;
}

HRESULT RollbackDriverTransaction(PCWSTR data, bool& reboot)
{
    DriverPlan plan;
    DRIVER_TRY(ReadDriverPlan(data, plan));
    std::vector<DriverUndoRecord> records;
    // A malformed tail must not prevent restoration of earlier complete attempted records.
    HRESULT first = LoadDriverRecords(plan, records, true);
    first = ReplayDriverInverseRecords(records, first, [&](DriverUndoRecord& record) -> std::int32_t {
        return RestoreDriverRecord(record, plan.installing, reboot);
    }, [&]() { DeleteDriverScript(plan); });
    if (FAILED(first)) WcaLog(LOGMSG_STANDARD, "Driver inverse failed (0x%x); own recovery script %ws will not be deleted.", first, plan.key.c_str());
    return first;
}

HRESULT CleanupDriverTransactions(PCWSTR data)
{
    LPWSTR copy = NULL;
    DRIVER_TRY(StrAllocString(&copy, data, 0));
    LPWSTR cursor = copy;
    HRESULT hr = S_OK;
    while (cursor && *cursor)
    {
        DriverPlan plan;
        hr = ScriptField(&cursor, plan.key);
        GUID guid;
        if (FAILED(hr) || plan.key.find(L"DrvTx-") != 0 || plan.key.size() != 44 || FAILED(CLSIDFromString(plan.key.c_str() + 6, &guid)))
        { hr = HRESULT_FROM_WIN32(ERROR_INVALID_DATA); break; }
        DeleteDriverScript(plan);
    }
    ReleaseStr(copy);
    return hr;
}
