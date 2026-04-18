#include "perception/WindowsUiAutomationProbe.h"

#include <algorithm>

#include <QFileInfo>
#include <QList>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>

#ifdef Q_OS_WIN
#include <windows.h>
#include <uiautomation.h>
#include <wrl/client.h>
#endif

namespace {
struct SanitizedCandidate
{
    QString value;
    bool redacted = false;
    QString redactionReason;
};

QString normalizedAppFamily(const QString &appId)
{
    QString normalized = QFileInfo(appId.trimmed()).completeBaseName().trimmed().toLower();
    if (normalized == QStringLiteral("code")) {
        return QStringLiteral("vscode");
    }
    if (normalized == QStringLiteral("msedge")) {
        return QStringLiteral("edge");
    }
    return normalized;
}

bool isBrowserApp(const QString &appId)
{
    static const QSet<QString> browserApps{
        QStringLiteral("chrome"),
        QStringLiteral("edge"),
        QStringLiteral("firefox"),
        QStringLiteral("brave"),
        QStringLiteral("opera")
    };
    return browserApps.contains(appId);
}

bool isEditorApp(const QString &appId)
{
    static const QSet<QString> editorApps{
        QStringLiteral("vscode"),
        QStringLiteral("cursor"),
        QStringLiteral("windsurf"),
        QStringLiteral("pycharm"),
        QStringLiteral("idea"),
        QStringLiteral("sublime_text")
    };
    return editorApps.contains(appId);
}

#ifdef Q_OS_WIN
QString controlTypeName(CONTROLTYPEID controlType)
{
    if (controlType == UIA_EditControlTypeId) {
        return QStringLiteral("edit");
    }
    if (controlType == UIA_DocumentControlTypeId) {
        return QStringLiteral("document");
    }
    if (controlType == UIA_TabItemControlTypeId) {
        return QStringLiteral("tab_item");
    }
    if (controlType == UIA_WindowControlTypeId) {
        return QStringLiteral("window");
    }
    if (controlType == UIA_PaneControlTypeId) {
        return QStringLiteral("pane");
    }
    return QStringLiteral("unknown");
}
#endif

QString safeCandidate(QString value)
{
    value = value.simplified();
    if (value.isEmpty() || value.size() > 96 || value.contains(QRegularExpression(QStringLiteral("[\\r\\n\\t]")))) {
        return {};
    }
    if (value.count(QLatin1Char(' ')) > 8) {
        return {};
    }
    return value;
}

SanitizedCandidate sanitizeCandidate(QString value)
{
    value = value.simplified();
    if (value.isEmpty()) {
        return {};
    }

    SanitizedCandidate candidate;
    if (value.size() > 96) {
        candidate.redacted = true;
        candidate.redactionReason = QStringLiteral("value_too_long");
        return candidate;
    }
    if (value.contains(QRegularExpression(QStringLiteral("[\\r\\n\\t]")))) {
        candidate.redacted = true;
        candidate.redactionReason = QStringLiteral("multiline_value_suppressed");
        return candidate;
    }
    if (value.count(QLatin1Char(' ')) > 8) {
        candidate.redacted = true;
        candidate.redactionReason = QStringLiteral("phrase_too_long");
        return candidate;
    }
    candidate.value = value;
    return candidate;
}

QString fileLikeCandidate(QString value)
{
    const SanitizedCandidate candidate = sanitizeCandidate(value);
    if (candidate.value.isEmpty()) {
        return {};
    }

    static const QRegularExpression filePattern(
        QStringLiteral("^[^\\\\/:*?\"<>|]+\\.(cpp|cc|c|h|hpp|py|md|txt|json|qml|js|ts|tsx|jsx|html|css|java|kt|rs|go|yml|yaml)$"),
        QRegularExpression::CaseInsensitiveOption);
    return filePattern.match(candidate.value).hasMatch() ? candidate.value : QString();
}

void noteRedaction(QVariantMap &metadata, const SanitizedCandidate &candidate)
{
    if (!candidate.redacted) {
        return;
    }

    metadata.insert(QStringLiteral("metadataRedacted"), true);
    if (!candidate.redactionReason.trimmed().isEmpty()) {
        metadata.insert(QStringLiteral("redactionReason"), candidate.redactionReason.trimmed());
    }
}

void finalizeMetadata(QVariantMap &metadata)
{
    const bool hasContext = !metadata.value(QStringLiteral("documentContext")).toString().trimmed().isEmpty()
        || !metadata.value(QStringLiteral("siteContext")).toString().trimmed().isEmpty()
        || !metadata.value(QStringLiteral("workspaceContext")).toString().trimmed().isEmpty();
    const bool redacted = metadata.value(QStringLiteral("metadataRedacted")).toBool();
    if (!hasContext && !redacted) {
        metadata.clear();
        return;
    }

    metadata.insert(QStringLiteral("automationSource"), QStringLiteral("uia"));
    double confidence = 0.58;
    if (!metadata.value(QStringLiteral("documentContext")).toString().trimmed().isEmpty()) {
        confidence += 0.18;
    }
    if (!metadata.value(QStringLiteral("siteContext")).toString().trimmed().isEmpty()
        || !metadata.value(QStringLiteral("workspaceContext")).toString().trimmed().isEmpty()) {
        confidence += 0.12;
    }
    if (redacted) {
        confidence -= 0.08;
    }
    confidence = std::clamp(confidence, 0.35, 0.92);
    metadata.insert(QStringLiteral("metadataConfidence"), confidence);
    metadata.insert(QStringLiteral("metadataQuality"),
                    confidence >= 0.8 ? QStringLiteral("high")
                    : confidence >= 0.65 ? QStringLiteral("medium")
                    : QStringLiteral("low"));
}

#ifdef Q_OS_WIN
QString fromBstr(BSTR value)
{
    if (value == nullptr) {
        return {};
    }
    const QString text = QString::fromWCharArray(value).simplified();
    SysFreeString(value);
    return text;
}

struct ComApartmentScope
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ~ComApartmentScope()
    {
        if (SUCCEEDED(hr)) {
            CoUninitialize();
        }
    }
};

struct ElementSnapshot
{
    QString name;
    QString value;
    CONTROLTYPEID controlType = 0;
    bool selected = false;
};

ElementSnapshot readElement(IUIAutomationElement *element)
{
    ElementSnapshot snapshot;
    if (element == nullptr) {
        return snapshot;
    }

    BSTR name = nullptr;
    if (SUCCEEDED(element->get_CurrentName(&name))) {
        snapshot.name = fromBstr(name);
    }

    element->get_CurrentControlType(&snapshot.controlType);

    Microsoft::WRL::ComPtr<IUIAutomationValuePattern> valuePattern;
    if (SUCCEEDED(element->GetCurrentPatternAs(UIA_ValuePatternId, IID_PPV_ARGS(&valuePattern))) && valuePattern) {
        BSTR value = nullptr;
        if (SUCCEEDED(valuePattern->get_CurrentValue(&value))) {
            snapshot.value = fromBstr(value);
        }
    }

    Microsoft::WRL::ComPtr<IUIAutomationSelectionItemPattern> selectionPattern;
    if (SUCCEEDED(element->GetCurrentPatternAs(UIA_SelectionItemPatternId, IID_PPV_ARGS(&selectionPattern))) && selectionPattern) {
        BOOL selected = FALSE;
        if (SUCCEEDED(selectionPattern->get_CurrentIsSelected(&selected))) {
            snapshot.selected = selected == TRUE;
        }
    }

    return snapshot;
}

void appendSafeName(QStringList &values, const QString &value, QVariantMap &metadata)
{
    const SanitizedCandidate candidate = sanitizeCandidate(value);
    noteRedaction(metadata, candidate);
    if (!candidate.value.isEmpty() && !values.contains(candidate.value, Qt::CaseInsensitive)) {
        values << candidate.value;
    }
}

void collectWindowElements(IUIAutomationTreeWalker *walker,
                           IUIAutomationElement *parent,
                           QList<ElementSnapshot> &snapshots,
                           int depth,
                           int &visited)
{
    if (walker == nullptr || parent == nullptr || depth > 5 || visited >= 72) {
        return;
    }

    Microsoft::WRL::ComPtr<IUIAutomationElement> child;
    if (FAILED(walker->GetFirstChildElement(parent, &child)) || !child) {
        return;
    }

    while (child && visited < 72) {
        ++visited;
        const ElementSnapshot snapshot = readElement(child.Get());
        if (!snapshot.name.trimmed().isEmpty() || !snapshot.value.trimmed().isEmpty()) {
            snapshots.push_back(snapshot);
        }
        collectWindowElements(walker, child.Get(), snapshots, depth + 1, visited);

        Microsoft::WRL::ComPtr<IUIAutomationElement> next;
        if (FAILED(walker->GetNextSiblingElement(child.Get(), &next))) {
            break;
        }
        child = next;
    }
}

QString firstUrlHost(const QList<ElementSnapshot> &elements)
{
    for (const ElementSnapshot &element : elements) {
        const QUrl url(element.value);
        if (url.isValid() && !url.host().trimmed().isEmpty()) {
            return url.host().trimmed().toLower();
        }
    }
    return {};
}

void appendWindowTreeNames(QStringList &names,
                           const QList<ElementSnapshot> &elements,
                           QVariantMap &metadata,
                           bool selectedTabsOnly)
{
    for (const ElementSnapshot &element : elements) {
        if (selectedTabsOnly && element.controlType == UIA_TabItemControlTypeId && !element.selected) {
            continue;
        }
        if (element.controlType == UIA_TabItemControlTypeId
            || element.controlType == UIA_DocumentControlTypeId
            || element.controlType == UIA_EditControlTypeId
            || element.controlType == UIA_PaneControlTypeId) {
            appendSafeName(names, element.name, metadata);
        }
    }
}

bool sameProcessWindow(HWND foregroundWindow, IUIAutomationElement *element)
{
    if (foregroundWindow == nullptr || element == nullptr) {
        return false;
    }

    UIA_HWND elementWindow = nullptr;
    if (FAILED(element->get_CurrentNativeWindowHandle(&elementWindow)) || elementWindow == nullptr) {
        return false;
    }

    DWORD foregroundPid = 0;
    DWORD elementPid = 0;
    GetWindowThreadProcessId(foregroundWindow, &foregroundPid);
    GetWindowThreadProcessId(reinterpret_cast<HWND>(elementWindow), &elementPid);
    return foregroundPid != 0 && foregroundPid == elementPid;
}
#endif
}

QVariantMap WindowsUiAutomationProbe::probeWindowMetadata(quintptr windowHandleValue,
                                                          const QString &appId)
{
    QVariantMap metadata;
#ifdef Q_OS_WIN
    const QString normalizedApp = normalizedAppFamily(appId);
    if ((!isBrowserApp(normalizedApp) && !isEditorApp(normalizedApp)) || windowHandleValue == 0) {
        return metadata;
    }

    ComApartmentScope apartment;
    if (FAILED(apartment.hr) && apartment.hr != RPC_E_CHANGED_MODE) {
        return metadata;
    }

    Microsoft::WRL::ComPtr<IUIAutomation> automation;
    if (FAILED(CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&automation))) || !automation) {
        return metadata;
    }

    Microsoft::WRL::ComPtr<IUIAutomationElement> focusedElement;
    const HWND foregroundWindow = reinterpret_cast<HWND>(windowHandleValue);
    const bool hasFocusedElement =
        SUCCEEDED(automation->GetFocusedElement(&focusedElement))
        && focusedElement
        && sameProcessWindow(foregroundWindow, focusedElement.Get());

    ElementSnapshot focused;
    if (hasFocusedElement) {
        focused = readElement(focusedElement.Get());
    }

    const SanitizedCandidate focusedName = sanitizeCandidate(focused.name);
    noteRedaction(metadata, focusedName);
    metadata.insert(QStringLiteral("focusedControlType"), controlTypeName(focused.controlType));
    if (!focusedName.value.isEmpty()) {
        metadata.insert(QStringLiteral("focusedElementName"), focusedName.value);
    }

    Microsoft::WRL::ComPtr<IUIAutomationTreeWalker> walker;
    if (FAILED(automation->get_ControlViewWalker(&walker)) || !walker) {
        return metadata;
    }

    Microsoft::WRL::ComPtr<IUIAutomationElement> rootElement;
    QList<ElementSnapshot> windowElements;
    if (SUCCEEDED(automation->ElementFromHandle(foregroundWindow, &rootElement)) && rootElement) {
        int visited = 0;
        collectWindowElements(walker.Get(), rootElement.Get(), windowElements, 0, visited);
        metadata.insert(QStringLiteral("metadataSource"), QStringLiteral("uia_window_tree"));
        metadata.insert(QStringLiteral("windowTreeElementCount"), windowElements.size());
    }

    QStringList names;
    if (!focusedName.value.isEmpty()) {
        names << focusedName.value;
    }

    if (hasFocusedElement) {
        Microsoft::WRL::ComPtr<IUIAutomationElement> current = focusedElement;
        for (int depth = 0; depth < 4 && current; ++depth) {
            Microsoft::WRL::ComPtr<IUIAutomationElement> parent;
            if (FAILED(walker->GetParentElement(current.Get(), &parent)) || !parent) {
                break;
            }
            current = parent;
            const SanitizedCandidate name = sanitizeCandidate(readElement(current.Get()).name);
            noteRedaction(metadata, name);
            if (!name.value.isEmpty() && !names.contains(name.value, Qt::CaseInsensitive)) {
                names << name.value;
            }
        }
    }
    appendWindowTreeNames(names, windowElements, metadata, isBrowserApp(normalizedApp));

    if (isBrowserApp(normalizedApp)) {
        QUrl url(focused.value);
        if (!url.isValid() || url.host().trimmed().isEmpty()) {
            const QString host = firstUrlHost(windowElements);
            if (!host.isEmpty()) {
                metadata.insert(QStringLiteral("siteContext"), host);
                metadata.insert(QStringLiteral("browserUrlHost"), host);
            }
        } else {
            metadata.insert(QStringLiteral("siteContext"), url.host().trimmed().toLower());
            metadata.insert(QStringLiteral("browserUrlScheme"), url.scheme().trimmed().toLower());
            metadata.insert(QStringLiteral("browserUrlHost"), url.host().trimmed().toLower());
        }
        for (const QString &name : names) {
            if (!name.contains(QStringLiteral("address"), Qt::CaseInsensitive)
                && !name.contains(QStringLiteral("search"), Qt::CaseInsensitive)
                && name.compare(metadata.value(QStringLiteral("siteContext")).toString(), Qt::CaseInsensitive) != 0) {
                metadata.insert(QStringLiteral("documentContext"), name);
                break;
            }
        }
        finalizeMetadata(metadata);
        metadata.insert(QStringLiteral("metadataClass"), QStringLiteral("browser_document"));
        metadata.insert(QStringLiteral("documentKind"), QStringLiteral("browser_page"));
        return metadata;
    }

    for (const QString &name : names) {
        const QString fileName = fileLikeCandidate(name);
        if (!fileName.isEmpty()) {
            metadata.insert(QStringLiteral("documentContext"), fileName);
            break;
        }
    }

    for (const QString &name : names) {
        if (name.compare(metadata.value(QStringLiteral("documentContext")).toString(), Qt::CaseInsensitive) != 0
            && !name.contains(QStringLiteral("visual studio"), Qt::CaseInsensitive)
            && !name.contains(QStringLiteral("cursor"), Qt::CaseInsensitive)
            && !name.contains(QStringLiteral("windsurf"), Qt::CaseInsensitive)) {
            metadata.insert(QStringLiteral("workspaceContext"), name);
            break;
        }
    }
    finalizeMetadata(metadata);
    if (!metadata.isEmpty()) {
        metadata.insert(QStringLiteral("metadataClass"), QStringLiteral("editor_document"));
        metadata.insert(QStringLiteral("documentKind"), QStringLiteral("editor_file"));
    }
#else
    Q_UNUSED(windowHandleValue);
    Q_UNUSED(appId);
#endif
    return metadata;
}
