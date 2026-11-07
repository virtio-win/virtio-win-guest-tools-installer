#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <limits>
#include <algorithm>
#include <utility>

struct DriverPackageIdentity
{
    std::wstring storeInf, infHash, catalogHash;
};

inline bool SameDriverPackage(const DriverPackageIdentity& a, const DriverPackageIdentity& b)
{
    return !a.infHash.empty() && a.infHash == b.infHash && a.catalogHash == b.catalogHash;
}

struct DriverMetadataValue
{
    bool present = false;
    std::wstring value;
};

struct DriverMetadata
{
    bool keyPresent = false;
    bool redHatPresent = false, virtioPresent = false, componentsPresent = false;
    // Only these module-owned REG_SZ fields are recorded; never arbitrary registry data.
    DriverMetadataValue oem, strongname, component, createdByComponent, infHash, catalogHash;
};

struct DriverBinding
{
    std::wstring instance;
    bool hasDriver = false;
    DriverPackageIdentity package;
    std::wstring section, sectionExt;
    bool attempted = false;
};

// SCM configuration of a service named by the source INF's AddService directives, captured before the
// package-wide install rewrites it (otherwise rollback leaves ImagePath inside the removed package).
struct DriverServiceConfig
{
    std::wstring name, imagePath, loadOrderGroup;
    unsigned type = 0, start = 0, error = 0;
    std::vector<std::wstring> dependencies;
};

// Compare only a successfully API-resolved actual section, never a driver's undecorated Models entry.
template<typename Equal>
inline bool DriverBindingSectionMatches(const DriverBinding& binding, const std::wstring& actual, Equal equal)
{
    std::wstring expected = binding.section;
    if (!binding.sectionExt.empty() && binding.sectionExt[0] != L'.') expected += L".";
    expected += binding.sectionExt;
    return !actual.empty() && equal(actual.c_str(), expected.c_str());
}

struct DriverUndoRecord
{
    std::wstring sourceInf, component, metadataKey;
    DriverPackageIdentity sourcePackage, stagedPackage;
    DriverMetadata metadata;
    std::vector<DriverBinding> bindings;
    std::vector<DriverServiceConfig> services;
    bool removeOwnedPackage = false;
    bool stageIntent = false;
    bool stageKnown = false;
    bool packageCreated = false;
    bool removalAttempted = false;
    bool metadataAttempted = false;
    std::wstring publishedInf;
};

// Encoding has fixed driver-specific fields; WiX supplies the on-disk string framing.
inline void EncodeDriverSnapshot(const DriverUndoRecord& r, std::vector<std::wstring>& fields)
{
    fields = { r.sourceInf, r.component, r.metadataKey,
        r.sourcePackage.storeInf, r.sourcePackage.infHash, r.sourcePackage.catalogHash,
        r.metadata.keyPresent ? L"1" : L"0",
        r.metadata.redHatPresent ? L"1" : L"0", r.metadata.virtioPresent ? L"1" : L"0",
        r.metadata.componentsPresent ? L"1" : L"0" };
    for (const auto* v : { &r.metadata.oem, &r.metadata.strongname,
        &r.metadata.component, &r.metadata.createdByComponent, &r.metadata.infHash, &r.metadata.catalogHash })
    {
        fields.push_back(v->present ? L"1" : L"0");
        fields.push_back(v->value);
    }
    fields.push_back(r.removeOwnedPackage ? L"1" : L"0");
    fields.push_back(std::to_wstring(r.bindings.size()));
    for (const auto& b : r.bindings)
    {
        fields.push_back(b.instance);
        fields.push_back(b.hasDriver ? L"1" : L"0");
        fields.push_back(b.package.storeInf);
        fields.push_back(b.package.infHash);
        fields.push_back(b.package.catalogHash);
        fields.push_back(b.section);
        fields.push_back(b.sectionExt);
    }
    fields.push_back(std::to_wstring(r.services.size()));
    for (const auto& s : r.services)
    {
        fields.push_back(s.name);
        fields.push_back(s.imagePath);
        fields.push_back(s.loadOrderGroup);
        fields.push_back(std::to_wstring(s.type));
        fields.push_back(std::to_wstring(s.start));
        fields.push_back(std::to_wstring(s.error));
        fields.push_back(std::to_wstring(s.dependencies.size()));
        fields.insert(fields.end(), s.dependencies.begin(), s.dependencies.end());
    }
}

inline bool DriverStateNumber(const std::wstring& s, unsigned& value)
{
    if (s.empty()) return false;
    value = 0;
    for (wchar_t c : s)
    {
        if (c < L'0' || c > L'9' || value > ((std::numeric_limits<unsigned>::max)() - (c - L'0')) / 10)
            return false;
        value = value * 10 + c - L'0';
    }
    return true;
}

inline bool DecodeDriverSnapshot(const std::vector<std::wstring>& f, DriverUndoRecord& r)
{
    if (f.size() < 24) return false;
    r = DriverUndoRecord();
    r.sourceInf = f[0]; r.component = f[1]; r.metadataKey = f[2];
    r.sourcePackage = { f[3], f[4], f[5] };
    auto bit = [](const std::wstring& s, bool& b) {
        if (s != L"0" && s != L"1") return false;
        b = s == L"1"; return true;
    };
    if (!bit(f[6], r.metadata.keyPresent) || !bit(f[7], r.metadata.redHatPresent) ||
        !bit(f[8], r.metadata.virtioPresent) || !bit(f[9], r.metadata.componentsPresent)) return false;
    if ((r.metadata.keyPresent && !r.metadata.componentsPresent) ||
        (r.metadata.componentsPresent && !r.metadata.virtioPresent) ||
        (r.metadata.virtioPresent && !r.metadata.redHatPresent)) return false;
    size_t at = 10;
    for (auto* v : { &r.metadata.oem, &r.metadata.strongname,
        &r.metadata.component, &r.metadata.createdByComponent, &r.metadata.infHash, &r.metadata.catalogHash })
    {
        if (!bit(f[at++], v->present)) return false;
        v->value = f[at++];
        if (!v->present && !v->value.empty()) return false;
    }
    if (!r.metadata.keyPresent && (r.metadata.oem.present || r.metadata.strongname.present ||
        r.metadata.component.present || r.metadata.createdByComponent.present || r.metadata.infHash.present || r.metadata.catalogHash.present)) return false;
    unsigned count;
    if (!bit(f[at++], r.removeOwnedPackage) || !DriverStateNumber(f[at++], count) ||
        count > (f.size() - at) / 7)
        return false;
    for (unsigned i = 0; i < count; ++i)
    {
        DriverBinding b;
        b.instance = f[at++];
        if (!bit(f[at++], b.hasDriver)) return false;
        b.package = { f[at], f[at+1], f[at+2] }; at += 3;
        b.section = f[at++]; b.sectionExt = f[at++];
        if (b.instance.empty() || (b.hasDriver && (b.package.infHash.empty() ||
            b.package.catalogHash.empty() || b.package.storeInf.empty() || b.section.empty()))) return false;
        if (!b.hasDriver && (!b.package.storeInf.empty() || !b.package.infHash.empty() ||
            !b.package.catalogHash.empty() || !b.section.empty() || !b.sectionExt.empty())) return false;
        r.bindings.push_back(b);
    }
    unsigned services;
    if (at >= f.size() || !DriverStateNumber(f[at++], services) || services > (f.size() - at) / 7) return false;
    for (unsigned i = 0; i < services; ++i)
    {
        if (f.size() - at < 7) return false;
        DriverServiceConfig s;
        s.name = f[at++]; s.imagePath = f[at++]; s.loadOrderGroup = f[at++];
        if (s.name.empty() || s.imagePath.empty() || !DriverStateNumber(f[at++], s.type) ||
            !DriverStateNumber(f[at++], s.start) || !DriverStateNumber(f[at++], s.error)) return false;
        unsigned dependencies;
        if (!DriverStateNumber(f[at++], dependencies) || dependencies > f.size() - at) return false;
        for (unsigned n = 0; n < dependencies; ++n)
        {
            const auto& dependency = f[at++];
            if (dependency.empty() || dependency.find(L'\0') != std::wstring::npos ||
                dependency.find(static_cast<wchar_t>(128)) != std::wstring::npos) return false;
            s.dependencies.push_back(dependency);
        }
        r.services.push_back(s);
    }
    return at == f.size() && !r.sourceInf.empty() && !r.component.empty() && !r.metadataKey.empty() &&
        !r.sourcePackage.infHash.empty() && !r.sourcePackage.catalogHash.empty();
}

// An authoritative staging result must precede any install binding/metadata effect.
inline bool RecordDriverStage(DriverUndoRecord& r, bool created,
    const DriverPackageIdentity& package, const std::wstring& published)
{
    if (r.stageKnown || published.empty() || !SameDriverPackage(r.sourcePackage, package)) return false;
    r.stageKnown = true; r.packageCreated = created; r.stagedPackage = package;
    r.publishedInf = published;
    return true;
}

// A written intent records a possible copy, not proof that this transaction created a package.
inline bool DriverStageIntentIncomplete(const DriverUndoRecord& record)
{
    return record.stageIntent && !record.stageKnown;
}

// Write the intent before the API call; a failed completion record must remain visible to rollback.
template<class StagePackage, class AppendEvent, class RemoveCreatedPackage>
std::int32_t StageDriverPackageWithIntent(DriverUndoRecord& record, unsigned index,
    StagePackage stage, AppendEvent append, RemoveCreatedPackage remove)
{
    if (record.stageIntent || record.stageKnown) return static_cast<std::int32_t>(0x8007000d);
    std::int32_t error = append({ L"stage-intent", std::to_wstring(index) });
    if (error < 0) return error;
    record.stageIntent = true;
    DriverPackageIdentity staged;
    std::wstring published;
    bool created = false;
    error = stage(staged, published, created);
    if (error >= 0)
        error = append({ L"stage", std::to_wstring(index), created ? L"1" : L"0",
            published, staged.storeInf, staged.infHash, staged.catalogHash });
    if (error < 0)
    {
        // Only the live API result proves this call created this exact published package.
        if (created && !published.empty()) remove(published);
        return error;
    }
    return RecordDriverStage(record, created, staged, published) ? 0 :
        static_cast<std::int32_t>(0x8007000d);
}

// Only a non-present devnode whose named package is absent from the store (not-found class errors) is NULL-driver.
inline bool DriverStalePhantomBinding(bool present, std::int32_t resolveHr)
{
    return !present && (resolveHr == static_cast<std::int32_t>(0x80070002) ||   // ERROR_FILE_NOT_FOUND
        resolveHr == static_cast<std::int32_t>(0x80070003));                    // ERROR_PATH_NOT_FOUND
}

struct DriverScopeNode
{
    DriverBinding binding;
    std::wstring parent;
    bool present = false;
};

// Install scope: present matching devnodes, plus non-present matching devnodes whose direct parent is a
// present in-scope node (a child the package-wide install may revive, e.g. VIOINPUT under PCI DEV_1052).
// Unrelated phantoms stay out. Output is parent-first, so the reverse-order inverse restores such a child
// while its parent still exposes it. Returns false (refuse before mutation) when present parents form a cycle.
template <typename Equal>
inline bool BuildDriverBindingScope(const std::vector<DriverScopeNode>& nodes, Equal equal, std::vector<DriverBinding>& scope)
{
    std::vector<size_t> kept;
    auto find = [&](const std::wstring& id, bool presentOnly) -> size_t {
        for (size_t i = 0; i < nodes.size(); ++i)
            if ((!presentOnly || nodes[i].present) && !id.empty() && equal(nodes[i].binding.instance.c_str(), id.c_str())) return i;
        return nodes.size();
    };
    for (size_t i = 0; i < nodes.size(); ++i)
        if (nodes[i].present || find(nodes[i].parent, true) != nodes.size()) kept.push_back(i);
    std::vector<unsigned> depth(nodes.size(), 0);
    for (size_t i : kept)
    {
        size_t at = i;
        for (size_t step = 0;; ++step)
        {
            if (step > nodes.size()) return false;
            size_t parent = find(nodes[at].parent, false);
            if (parent == nodes.size() || std::find(kept.begin(), kept.end(), parent) == kept.end()) break;
            ++depth[i];
            at = parent;
        }
    }
    std::stable_sort(kept.begin(), kept.end(), [&](size_t a, size_t b) { return depth[a] < depth[b]; });
    scope.clear();
    for (size_t i : kept) scope.push_back(nodes[i].binding);
    return true;
}

// DiInstallDriverW (flags 0) fails with ERROR_NO_MORE_ITEMS after importing the package when no present
// device needs it (none better). The caller still verifies the staged identity and every captured binding.
static constexpr unsigned long DriverNoDeviceUpdated = 259;
inline bool DriverPackageInstallAccepted(bool installed, unsigned long error)
{
    return installed || error == DriverNoDeviceUpdated;
}

// Hashes are identity; locations and OEM aliases only accelerate read-only lookup.
inline bool KnownDriverPackageIdentity(const DriverPackageIdentity& identity)
{
    for (const auto* hash : { &identity.infHash, &identity.catalogHash })
    {
        if (hash->size() != 64) return false;
        for (wchar_t c : *hash) if (!((c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f'))) return false;
    }
    return true;
}

inline DriverPackageIdentity MetadataPackageIdentity(const DriverMetadata& metadata)
{
    return { metadata.strongname.present ? metadata.strongname.value : L"",
        metadata.infHash.present ? metadata.infHash.value : L"",
        metadata.catalogHash.present ? metadata.catalogHash.value : L"" };
}

// Raw RegGetValue status is checked before a retained candidate can be classified as unshared.
// Only ERROR_FILE_NOT_FOUND (2) means intentionally absent/unhashed; malformed data is ERROR_INVALID_DATA (13).
template<class ReadHash, class NoMatchingReference>
std::uint32_t CheckRetainedDriverMetadataHash(const DriverPackageIdentity& expected, ReadHash read,
    bool& known, NoMatchingReference unshared)
{
    known = false;
    DriverPackageIdentity identity;
    bool missing = false;
    const wchar_t* names[] = { L"infHash", L"catalogHash" };
    std::wstring* values[] = { &identity.infHash, &identity.catalogHash };
    for (unsigned i = 0; i < 2; ++i)
    {
        std::uint32_t error = read(names[i], *values[i]);
        if (error == 2) { missing = true; continue; }
        if (error) return error;
        if (values[i]->size() != 64) return 13;
        for (wchar_t c : *values[i])
            if (!((c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f'))) return 13;
    }
    known = !missing;
    if (known && !SameDriverPackage(identity, expected)) unshared();
    return 0;
}

struct DriverPublishedPackage { std::wstring published; DriverPackageIdentity identity; };
enum class DriverResolution { found, missing, ambiguous, invalidIdentity, readError };

// Used by native resolution and injected tests. Never promote a location hint into identity.
template<class LookupHint, class ScanPublished>
DriverResolution ResolveDriverPackageIdentity(const DriverPackageIdentity& expected,
    const std::vector<std::wstring>& hints, LookupHint lookup, ScanPublished scan, DriverPublishedPackage& result)
{
    if (!KnownDriverPackageIdentity(expected)) return DriverResolution::invalidIdentity;
    std::vector<DriverPublishedPackage> matches;
    auto accept = [&](const DriverPublishedPackage& candidate) {
        if (!SameDriverPackage(expected, candidate.identity)) return;
        for (const auto& match : matches) if (match.published == candidate.published) return;
        matches.push_back(candidate);
    };
    for (const auto& hint : hints)
    {
        DriverPublishedPackage candidate;
        if (!hint.empty() && lookup(hint, candidate)) accept(candidate);
    }
    if (matches.empty())
    {
        std::vector<DriverPublishedPackage> candidates;
        if (!scan(candidates)) return DriverResolution::readError;
        for (const auto& candidate : candidates) accept(candidate);
    }
    if (matches.empty()) return DriverResolution::missing;
    if (matches.size() != 1) return DriverResolution::ambiguous;
    result = matches.front();
    return DriverResolution::found;
}

struct DriverPlanEntry { std::wstring source, component; unsigned flags = 0; bool soleOwner = false; };
struct DriverPlan { std::wstring key, product; bool installing = false; std::vector<DriverPlanEntry> entries; };
static constexpr wchar_t DriverJournalSchema[] = L"VirtioDriverTransaction6";

inline std::wstring DriverMetadataKey(const std::wstring& component)
{
    return L"SOFTWARE\\RedHat\\Virtio-Win\\Components\\" + component;
}

inline bool SameDriverComponent(std::wstring a, std::wstring b)
{
    for (auto* text : { &a, &b })
        for (auto& c : *text) if (c >= L'a' && c <= L'f') c = static_cast<wchar_t>(c - (L'a' - L'A'));
    return a == b;
}

enum class DriverPrefixStatus { complete, corrupt, readError };
struct DriverScriptPrefix { std::wstring text; DriverPrefixStatus status = DriverPrefixStatus::complete; };

// The script is UTF-16LE on supported native x86/x64. Keep whole WCHARs only, without Unicode reinterpretation.
template<class ReadBytes>
DriverScriptPrefix ReadDriverScriptPrefix(unsigned byteCount, ReadBytes readBytes)
{
    DriverScriptPrefix prefix;
    if (byteCount % 2) prefix.status = DriverPrefixStatus::corrupt;
    std::vector<unsigned char> bytes(byteCount);
    unsigned total = 0;
    while (total < byteCount)
    {
        unsigned got = 0;
        unsigned wanted = (std::min)(byteCount - total, 16384u);
        bool ok = readBytes(bytes.data() + total, wanted, got);
        if (got > wanted) { if (prefix.status == DriverPrefixStatus::complete) prefix.status = DriverPrefixStatus::readError; break; }
        total += got;
        if (!ok || !got)
        { if (prefix.status == DriverPrefixStatus::complete) prefix.status = DriverPrefixStatus::readError; break; }
    }
    prefix.text.reserve(total / 2);
    for (unsigned i = 0; i + 1 < total; i += 2)
        prefix.text.push_back(static_cast<wchar_t>(bytes[i] | (static_cast<unsigned>(bytes[i+1]) << 8)));
    return prefix;
}

inline bool ReadDriverJournalEvent(const std::wstring& text, size_t& at, std::vector<std::wstring>& event)
{
    auto field = [&](std::wstring& value) {
        if (at >= text.size()) return false;
        size_t end = text.find(static_cast<wchar_t>(128), at);
        value = text.substr(at, end == std::wstring::npos ? end : end - at);
        at = end == std::wstring::npos ? text.size() : end + 1;
        return value.size() <= 32767 && value.find(L'\0') == std::wstring::npos;
    };
    std::wstring count, end;
    unsigned size;
    if (!field(count) || !DriverStateNumber(count, size) || !size || size > 1048576) return false;
    event.clear();
    for (unsigned i = 0; i < size; ++i)
    {
        std::wstring value;
        if (!field(value)) return false;
        event.push_back(value);
    }
    return field(end) && end == L"complete";
}

// Only complete frames change replay state; earlier completed intents survive a malformed later frame.
inline bool ParseDriverJournal(const DriverPlan& plan, const std::wstring& text, std::vector<DriverUndoRecord>& records)
{
    size_t at = 0;
    std::vector<std::wstring> event;
    if (!ReadDriverJournalEvent(text, at, event) || event.size() != 4 || event[0] != DriverJournalSchema ||
        event[1] != plan.key || event[2] != (plan.installing ? L"1" : L"0") ||
        event[3] != std::to_wstring(plan.entries.size())) return false;
    while (at < text.size())
    {
        if (!ReadDriverJournalEvent(text, at, event)) return false;
        unsigned index = 0, binding = 0;
        bool valid = false;
        if (event[0] == L"snapshot" && records.size() < plan.entries.size())
        {
            DriverUndoRecord record;
            valid = DecodeDriverSnapshot(std::vector<std::wstring>(event.begin()+1, event.end()), record);
            const auto& entry = plan.entries[records.size()];
            valid = valid && record.sourceInf == entry.source && record.component == entry.component &&
                record.metadataKey == DriverMetadataKey(entry.component) &&
                (!record.removeOwnedPackage || (!plan.installing && entry.soleOwner &&
                    record.metadata.component.present && record.metadata.createdByComponent.present &&
                    KnownDriverPackageIdentity(MetadataPackageIdentity(record.metadata)) &&
                    SameDriverComponent(record.metadata.component.value, record.component) &&
                    SameDriverComponent(record.metadata.createdByComponent.value, record.component)));
            if (valid) records.push_back(record);
        }
        else if (event.size() >= 2 && DriverStateNumber(event[1], index) && index < records.size() && index+1 == records.size())
        {
            auto& record = records[index];
            if (event[0] == L"stage-intent" && plan.installing && event.size() == 2 &&
                !record.stageIntent && !record.stageKnown)
            { record.stageIntent = true; valid = true; }
            else if (event[0] == L"stage" && plan.installing && record.stageIntent && event.size() == 7 && (event[2] == L"0" || event[2] == L"1"))
                valid = RecordDriverStage(record, event[2] == L"1", { event[4], event[5], event[6] }, event[3]);
            else if (event[0] == L"bind" && plan.installing && record.stageKnown && !record.metadataAttempted &&
                event.size() == 3 && DriverStateNumber(event[2], binding) && binding < record.bindings.size() && !record.bindings[binding].attempted)
            { record.bindings[binding].attempted = true; valid = true; }
            else if (event[0] == L"remove" && !plan.installing && event.size() == 2 && record.removeOwnedPackage && !record.removalAttempted && !record.metadataAttempted)
            { record.removalAttempted = true; valid = true; }
            else if (event[0] == L"metadata" && event.size() == 2 && !record.metadataAttempted &&
                (plan.installing ? record.stageKnown : (!record.removeOwnedPackage || record.removalAttempted)))
            { record.metadataAttempted = true; valid = true; }
        }
        if (!valid) return false;
    }
    return text.empty() || text.back() != static_cast<wchar_t>(128);
}

template<class RestoreRecord, class DeleteOwnScript>
std::int32_t ReplayDriverInverseRecords(std::vector<DriverUndoRecord>& records, std::int32_t first,
    RestoreRecord restore, DeleteOwnScript cleanup)
{
    for (auto it = records.rbegin(); it != records.rend(); ++it)
    {
        std::int32_t error = restore(*it);
        if (error < 0 && first >= 0) first = error;
    }
    if (first >= 0) cleanup();
    return first;
}

#ifdef _WIN32
HRESULT BeginDriverTransaction(PCWSTR data);
HRESULT ExecuteDriverTransaction(PCWSTR data, bool& reboot);
HRESULT RollbackDriverTransaction(PCWSTR data, bool& reboot);
HRESULT CleanupDriverTransactions(PCWSTR data);
#endif
