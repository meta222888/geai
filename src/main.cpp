#include <windows.h>
#include <windowsx.h>
#include <winhttp.h>
#include <commctrl.h>

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "comctl32.lib")

#define IDC_SESSION_LIST 1000
#define IDC_CHAT 1001
#define IDC_INPUT 1002
#define IDC_SEND 1003
#define IDC_NEW 1004
#define IDC_SETTINGS 1005
#define IDM_DELETE_SESSION 3001
#define WM_GEAI_RESPONSE (WM_APP + 1)

static const wchar_t* SEND_BUTTON_TEXT = L"Send\r\nCtrl+Enter";
static const wchar_t* THINKING_BUTTON_TEXT = L"Gemini\r\nThinking...";
static const size_t MAX_CONTEXT_MESSAGES = 20;

struct Config {
    std::wstring apiKey;
    std::wstring apiBase = L"https://generativelanguage.googleapis.com";
    std::wstring model = L"gemini-2.5-flash";
    std::wstring proxy;
};

struct Message { std::wstring role; std::wstring text; };
struct SessionItem { std::wstring title; std::wstring path; std::filesystem::file_time_type modified; };
struct PendingResponse { std::wstring text; };

static HINSTANCE gInst;
static HWND gMain, gSidebar, gSessionList, gChat, gInput, gSendBtn, gNewBtn, gSettingsBtn;
static WNDPROC gInputOldProc = nullptr;
static HFONT gFont;
static HBRUSH gBgBrush, gPanelBrush, gInputBrush;
static Config gConfig;
static std::vector<Message> gMessages;
static std::vector<SessionItem> gSessions;
static std::wstring gCurrentSessionPath;
static std::wstring gRightClickedSessionPath;
static bool gRequestInFlight = false;

void SendPrompt();

std::wstring NormalizeNewlines(const std::wstring& s) {
    std::wstring out;
    out.reserve(s.size() + 16);
    for (size_t i = 0; i < s.size(); ++i) {
        wchar_t ch = s[i];
        if (ch == L'\r') {
            if (i + 1 < s.size() && s[i + 1] == L'\n') { out += L"\r\n"; ++i; }
            else out += L"\r\n";
        } else if (ch == L'\n') out += L"\r\n";
        else out += ch;
    }
    return out;
}

std::wstring AppDir() {
    wchar_t* appData = nullptr;
    size_t len = 0;
    _wdupenv_s(&appData, &len, L"APPDATA");
    std::wstring dir = appData ? appData : L".";
    if (appData) free(appData);
    dir += L"\\Geai";
    std::filesystem::create_directories(dir + L"\\sessions");
    return dir;
}

std::wstring SessionsDir() { return AppDir() + L"\\sessions"; }
std::wstring ConfigPath() { return AppDir() + L"\\config.ini"; }

std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), n, nullptr, nullptr);
    return s;
}

std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    return w;
}

std::string JsonEscape(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': break;
        case '\t': out += "\\t"; break;
        default: out += c; break;
        }
    }
    return out;
}

std::string JsonUnescape(const std::string& s) {
    std::string out;
    bool esc = false;
    for (char c : s) {
        if (esc) {
            if (c == 'n') out += '\n';
            else if (c == 't') out += '\t';
            else if (c == 'r') out += '\r';
            else if (c == '"') out += '"';
            else if (c == '\\') out += '\\';
            else out += c;
            esc = false;
        } else if (c == '\\') esc = true;
        else out += c;
    }
    return out;
}

std::string ExtractJsonString(const std::string& json, const std::string& key, size_t start = 0) {
    std::string token = "\"" + key + "\"";
    size_t pos = json.find(token, start);
    while (pos != std::string::npos) {
        pos += token.size();
        while (pos < json.size() && isspace((unsigned char)json[pos])) pos++;
        if (pos < json.size() && json[pos] == ':') {
            pos++;
            while (pos < json.size() && isspace((unsigned char)json[pos])) pos++;
            if (pos < json.size() && json[pos] == '"') {
                pos++;
                std::string out;
                bool esc = false;
                for (; pos < json.size(); ++pos) {
                    char c = json[pos];
                    if (esc) { out += '\\'; out += c; esc = false; }
                    else if (c == '\\') esc = true;
                    else if (c == '"') return JsonUnescape(out);
                    else out += c;
                }
            }
        }
        pos = json.find(token, pos);
    }
    return {};
}

std::wstring ExtractGeminiReply(const std::string& body) {
    if (body.empty()) return L"";
    if (body.find("\"candidates\"") == std::string::npos && body.find("\"error\"") == std::string::npos) return NormalizeNewlines(Utf8ToWide(body));
    std::string text;
    size_t pos = 0;
    while (true) {
        auto t = ExtractJsonString(body, "text", pos);
        if (t.empty()) break;
        if (!text.empty()) text += "\n";
        text += t;
        auto found = body.find("\"text\"", pos);
        if (found == std::string::npos) break;
        pos = found + 6;
    }
    if (!text.empty()) return NormalizeNewlines(Utf8ToWide(text));
    auto msg = ExtractJsonString(body, "message");
    if (!msg.empty()) return NormalizeNewlines(Utf8ToWide("Error: " + msg));
    return NormalizeNewlines(Utf8ToWide(body));
}

std::string BuildGeminiRequestBody(const std::vector<Message>& messages) {
    std::string body = "{\"contents\":[";
    bool first = true;
    size_t start = messages.size() > MAX_CONTEXT_MESSAGES ? messages.size() - MAX_CONTEXT_MESSAGES : 0;
    for (size_t i = start; i < messages.size(); ++i) {
        const auto& m = messages[i];
        if (m.text.empty()) continue;
        std::string role;
        if (m.role == L"assistant") role = "model";
        else if (m.role == L"user") role = "user";
        else continue;
        if (!first) body += ",";
        first = false;
        body += "{\"role\":\"" + role + "\",\"parts\":[{\"text\":\"" + JsonEscape(WideToUtf8(m.text)) + "\"}]}";
    }
    body += "]}";
    return body;
}

std::wstring TrimTitle(std::wstring s) {
    for (auto& ch : s) if (ch == L'\r' || ch == L'\n' || ch == L'\t') ch = L' ';
    while (!s.empty() && s.front() == L' ') s.erase(s.begin());
    if (s.empty()) return L"New chat";
    if (s.size() > 28) s = s.substr(0, 28) + L"...";
    return s;
}

std::wstring ReadIniValue(const wchar_t* section, const wchar_t* key, const wchar_t* def) {
    wchar_t buf[4096]{};
    GetPrivateProfileStringW(section, key, def, buf, 4096, ConfigPath().c_str());
    return buf;
}

void LoadConfig() {
    gConfig.apiKey = ReadIniValue(L"gemini", L"api_key", L"");
    gConfig.apiBase = ReadIniValue(L"gemini", L"api_base", L"https://generativelanguage.googleapis.com");
    gConfig.model = ReadIniValue(L"gemini", L"model", L"gemini-2.5-flash");
    gConfig.proxy = ReadIniValue(L"network", L"proxy", L"");
}

void SaveConfig() {
    auto p = ConfigPath();
    WritePrivateProfileStringW(L"gemini", L"api_key", gConfig.apiKey.c_str(), p.c_str());
    WritePrivateProfileStringW(L"gemini", L"api_base", gConfig.apiBase.c_str(), p.c_str());
    WritePrivateProfileStringW(L"gemini", L"model", gConfig.model.c_str(), p.c_str());
    WritePrivateProfileStringW(L"network", L"proxy", gConfig.proxy.c_str(), p.c_str());
}

void SetControlFont(HWND h) { SendMessageW(h, WM_SETFONT, (WPARAM)gFont, TRUE); }

void AppendChat(const std::wstring& role, const std::wstring& text) {
    std::wstring name = role == L"user" ? L"You" : (role == L"assistant" ? L"Gemini" : L"Geai");
    int len = GetWindowTextLengthW(gChat);
    SendMessageW(gChat, EM_SETSEL, len, len);
    std::wstring line = L"\r\n" + name + L"\r\n" + NormalizeNewlines(text) + L"\r\n";
    SendMessageW(gChat, EM_REPLACESEL, FALSE, (LPARAM)line.c_str());
}

void RenderChat() {
    SetWindowTextW(gChat, L"");
    for (const auto& m : gMessages) AppendChat(m.role, m.text);
}

std::wstring MakeSessionPath() {
    std::time_t t = std::time(nullptr);
    return SessionsDir() + L"\\session_" + std::to_wstring((long long)t) + L".jsonl";
}

void SaveCurrentSession() {
    if (gMessages.empty()) return;
    if (gCurrentSessionPath.empty()) gCurrentSessionPath = MakeSessionPath();
    std::ofstream f(gCurrentSessionPath, std::ios::binary | std::ios::trunc);
    for (const auto& m : gMessages) {
        f << "{\"role\":\"" << WideToUtf8(m.role) << "\",\"text\":\"" << JsonEscape(WideToUtf8(m.text)) << "\"}\n";
    }
}

std::vector<Message> ReadSessionMessages(const std::wstring& path) {
    std::vector<Message> messages;
    std::ifstream f(path, std::ios::binary);
    std::string line;
    while (std::getline(f, line)) {
        auto role = Utf8ToWide(ExtractJsonString(line, "role"));
        auto text = Utf8ToWide(ExtractJsonString(line, "text"));
        if (!role.empty()) messages.push_back({ role, text });
    }
    return messages;
}

std::wstring SessionTitleFromFile(const std::wstring& path) {
    auto messages = ReadSessionMessages(path);
    for (const auto& m : messages) if (m.role == L"user" && !m.text.empty()) return TrimTitle(m.text);
    return L"New chat";
}

void RefreshSessionList() {
    gSessions.clear();
    std::filesystem::create_directories(SessionsDir());
    for (const auto& entry : std::filesystem::directory_iterator(SessionsDir())) {
        if (!entry.is_regular_file() || entry.path().extension() != L".jsonl") continue;
        gSessions.push_back({ SessionTitleFromFile(entry.path().wstring()), entry.path().wstring(), entry.last_write_time() });
    }
    std::sort(gSessions.begin(), gSessions.end(), [](const SessionItem& a, const SessionItem& b) { return a.modified > b.modified; });
    SendMessageW(gSessionList, LB_RESETCONTENT, 0, 0);
    for (const auto& s : gSessions) SendMessageW(gSessionList, LB_ADDSTRING, 0, (LPARAM)s.title.c_str());
}

void SelectCurrentSessionInList() {
    for (size_t i = 0; i < gSessions.size(); ++i) {
        if (gSessions[i].path == gCurrentSessionPath) {
            SendMessageW(gSessionList, LB_SETCURSEL, (WPARAM)i, 0);
            return;
        }
    }
    SendMessageW(gSessionList, LB_SETCURSEL, (WPARAM)-1, 0);
}

void LoadSessionByIndex(int index) {
    if (index < 0 || index >= (int)gSessions.size()) return;
    if (gRequestInFlight) return;
    SaveCurrentSession();
    gCurrentSessionPath = gSessions[index].path;
    gMessages = ReadSessionMessages(gCurrentSessionPath);
    RenderChat();
    SelectCurrentSessionInList();
}

void NewSession() {
    if (gRequestInFlight) return;
    SaveCurrentSession();
    gMessages.clear();
    gCurrentSessionPath.clear();
    SetWindowTextW(gChat, L"");
    SetWindowTextW(gInput, L"");
    SendMessageW(gSessionList, LB_SETCURSEL, (WPARAM)-1, 0);
    SetFocus(gInput);
}

void ClearCurrentSessionUi() {
    gMessages.clear();
    gCurrentSessionPath.clear();
    SetWindowTextW(gChat, L"");
    SetWindowTextW(gInput, L"");
}

void DeleteSessionByPath(const std::wstring& path) {
    if (gRequestInFlight || path.empty()) return;
    if (MessageBoxW(gMain, L"Delete this conversation?", L"Geai", MB_YESNO | MB_ICONQUESTION) != IDYES) return;

    bool deletingCurrent = (path == gCurrentSessionPath);
    std::error_code ec;
    std::filesystem::remove(path, ec);

    if (ec) {
        MessageBoxW(gMain, (L"Delete failed:\n" + Utf8ToWide(ec.message())).c_str(), L"Geai", MB_OK | MB_ICONERROR);
        return;
    }

    if (deletingCurrent) ClearCurrentSessionUi();
    RefreshSessionList();
    SelectCurrentSessionInList();
}

bool ParseUrl(const std::wstring& url, URL_COMPONENTS& uc, std::wstring& host, std::wstring& path) {
    ZeroMemory(&uc, sizeof(uc));
    uc.dwStructSize = sizeof(uc);
    wchar_t h[512]{}, p[4096]{};
    uc.lpszHostName = h; uc.dwHostNameLength = 512;
    uc.lpszUrlPath = p; uc.dwUrlPathLength = 4096;
    uc.dwSchemeLength = 1;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) return false;
    host.assign(h, uc.dwHostNameLength);
    path.assign(p, uc.dwUrlPathLength);
    return true;
}

std::wstring CallGeminiWithConfig(const std::vector<Message>& context, Config cfg) {
    std::wstring base = cfg.apiBase;
    if (!base.empty() && base.back() == L'/') base.pop_back();
    bool official = base.find(L"generativelanguage.googleapis.com") != std::wstring::npos;
    std::wstring url = base + L"/v1beta/models/" + cfg.model + L":generateContent";
    if (official) url += L"?key=" + cfg.apiKey;

    URL_COMPONENTS uc{};
    std::wstring host, path;
    if (!ParseUrl(url, uc, host, path)) return L"Invalid API Base URL.";
    if (uc.lpszExtraInfo) path += uc.lpszExtraInfo;

    DWORD access = cfg.proxy.empty() ? WINHTTP_ACCESS_TYPE_DEFAULT_PROXY : WINHTTP_ACCESS_TYPE_NAMED_PROXY;
    HINTERNET hSession = WinHttpOpen(L"Geai/0.5", access, cfg.proxy.empty() ? WINHTTP_NO_PROXY_NAME : cfg.proxy.c_str(), WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return L"WinHttpOpen failed.";
    WinHttpSetTimeouts(hSession, 15000, 15000, 30000, 60000);
    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), uc.nPort, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return L"WinHttpConnect failed."; }
    DWORD flags = uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return L"WinHttpOpenRequest failed."; }

    std::string body = BuildGeminiRequestBody(context);
    std::wstring headers = L"Content-Type: application/json\r\n";
    if (!official && !cfg.apiKey.empty()) headers += L"X-Geai-Token: " + cfg.apiKey + L"\r\n";

    BOOL ok = WinHttpSendRequest(hRequest, headers.c_str(), -1, (LPVOID)body.data(), (DWORD)body.size(), (DWORD)body.size(), 0);
    if (ok) ok = WinHttpReceiveResponse(hRequest, nullptr);

    std::string response;
    if (ok) {
        DWORD size = 0;
        do {
            WinHttpQueryDataAvailable(hRequest, &size);
            if (!size) break;
            std::string buf(size, 0);
            DWORD read = 0;
            WinHttpReadData(hRequest, buf.data(), size, &read);
            response.append(buf.data(), read);
        } while (size > 0);
    }
    WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
    if (!ok) return L"Request failed.";
    return ExtractGeminiReply(response);
}

INT_PTR CALLBACK SettingsProc(HWND dlg, UINT msg, WPARAM wp, LPARAM) {
    switch (msg) {
    case WM_INITDIALOG:
        SetDlgItemTextW(dlg, 2001, gConfig.apiKey.c_str());
        SetDlgItemTextW(dlg, 2002, gConfig.apiBase.c_str());
        SetDlgItemTextW(dlg, 2003, gConfig.model.c_str());
        SetDlgItemTextW(dlg, 2004, gConfig.proxy.c_str());
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK) {
            wchar_t buf[4096]{};
            GetDlgItemTextW(dlg, 2001, buf, 4096); gConfig.apiKey = buf;
            GetDlgItemTextW(dlg, 2002, buf, 4096); gConfig.apiBase = buf;
            GetDlgItemTextW(dlg, 2003, buf, 4096); gConfig.model = buf;
            GetDlgItemTextW(dlg, 2004, buf, 4096); gConfig.proxy = buf;
            SaveConfig(); EndDialog(dlg, IDOK); return TRUE;
        }
        if (LOWORD(wp) == IDCANCEL) { EndDialog(dlg, IDCANCEL); return TRUE; }
    }
    return FALSE;
}

void SendPrompt() {
    if (gRequestInFlight) return;
    int len = GetWindowTextLengthW(gInput);
    if (len <= 0) return;
    std::wstring prompt(len, 0);
    GetWindowTextW(gInput, prompt.data(), len + 1);
    SetWindowTextW(gInput, L"");
    gMessages.push_back({ L"user", prompt });
    AppendChat(L"user", prompt);
    SaveCurrentSession(); RefreshSessionList(); SelectCurrentSessionInList();

    gRequestInFlight = true;
    EnableWindow(gSendBtn, FALSE);
    EnableWindow(gInput, FALSE);
    SetWindowTextW(gSendBtn, THINKING_BUTTON_TEXT);
    Config cfg = gConfig;
    HWND hwnd = gMain;
    std::vector<Message> context = gMessages;
    std::thread([context, cfg, hwnd]() {
        auto* result = new PendingResponse{ CallGeminiWithConfig(context, cfg) };
        PostMessageW(hwnd, WM_GEAI_RESPONSE, 0, (LPARAM)result);
    }).detach();
}

LRESULT CALLBACK InputProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_KEYDOWN && wp == VK_RETURN && (GetKeyState(VK_CONTROL) & 0x8000)) {
        SendPrompt();
        return 0;
    }
    return CallWindowProcW(gInputOldProc, hwnd, msg, wp, lp);
}

void Layout(HWND hwnd) {
    RECT rc{}; GetClientRect(hwnd, &rc);
    int w = rc.right, h = rc.bottom, sideW = 260, pad = 14, inputH = 86;
    MoveWindow(gSidebar, 0, 0, sideW, h, TRUE);
    MoveWindow(gNewBtn, 14, 14, 112, 32, TRUE);
    MoveWindow(gSettingsBtn, 136, 14, 110, 32, TRUE);
    MoveWindow(gSessionList, 12, 58, sideW - 24, h - 70, TRUE);
    int x = sideW + pad, rw = w - sideW - pad * 2;
    MoveWindow(gChat, x, pad, rw, h - inputH - pad * 3, TRUE);
    MoveWindow(gInput, x, h - inputH - pad, rw - 132, inputH, TRUE);
    MoveWindow(gSendBtn, x + rw - 120, h - inputH - pad, 120, inputH, TRUE);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        gSidebar = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 0, 0, 260, 600, hwnd, nullptr, gInst, nullptr);
        gNewBtn = CreateWindowW(L"BUTTON", L"+ New chat", WS_CHILD | WS_VISIBLE, 14, 14, 112, 32, hwnd, (HMENU)IDC_NEW, gInst, nullptr);
        gSettingsBtn = CreateWindowW(L"BUTTON", L"Settings", WS_CHILD | WS_VISIBLE, 136, 14, 110, 32, hwnd, (HMENU)IDC_SETTINGS, gInst, nullptr);
        gSessionList = CreateWindowExW(0, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT, 12, 58, 236, 520, hwnd, (HMENU)IDC_SESSION_LIST, gInst, nullptr);
        gChat = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, 274, 14, 700, 420, hwnd, (HMENU)IDC_CHAT, gInst, nullptr);
        gInput = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL, 274, 448, 580, 86, hwnd, (HMENU)IDC_INPUT, gInst, nullptr);
        gSendBtn = CreateWindowW(L"BUTTON", SEND_BUTTON_TEXT, WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | BS_MULTILINE, 868, 448, 120, 86, hwnd, (HMENU)IDC_SEND, gInst, nullptr);
        gInputOldProc = (WNDPROC)SetWindowLongPtrW(gInput, GWLP_WNDPROC, (LONG_PTR)InputProc);
        SetControlFont(gNewBtn); SetControlFont(gSettingsBtn); SetControlFont(gSessionList); SetControlFont(gChat); SetControlFont(gInput); SetControlFont(gSendBtn);
        RefreshSessionList(); Layout(hwnd); return 0;
    case WM_GEAI_RESPONSE: {
        PendingResponse* result = (PendingResponse*)lp;
        std::wstring answer = result ? result->text : L"Request failed.";
        delete result;
        gMessages.push_back({ L"assistant", answer });
        AppendChat(L"assistant", answer);
        SaveCurrentSession(); RefreshSessionList(); SelectCurrentSessionInList();
        gRequestInFlight = false;
        EnableWindow(gInput, TRUE);
        EnableWindow(gSendBtn, TRUE);
        SetWindowTextW(gSendBtn, SEND_BUTTON_TEXT);
        SetFocus(gInput);
        return 0;
    }
    case WM_SIZE: Layout(hwnd); return 0;
    case WM_COMMAND:
        if (LOWORD(wp) == IDC_SEND) SendPrompt();
        else if (LOWORD(wp) == IDC_NEW) NewSession();
        else if (LOWORD(wp) == IDC_SETTINGS) DialogBoxW(gInst, MAKEINTRESOURCEW(101), hwnd, SettingsProc);
        else if (LOWORD(wp) == IDC_SESSION_LIST && HIWORD(wp) == LBN_SELCHANGE) LoadSessionByIndex((int)SendMessageW(gSessionList, LB_GETCURSEL, 0, 0));
        else if (LOWORD(wp) == IDM_DELETE_SESSION) DeleteSessionByPath(gRightClickedSessionPath);
        return 0;
    case WM_CONTEXTMENU:
        if ((HWND)wp == gSessionList) {
            POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) }, client = pt;
            ScreenToClient(gSessionList, &client);
            int hit = (int)SendMessageW(gSessionList, LB_ITEMFROMPOINT, 0, MAKELPARAM(client.x, client.y));
            int idx = LOWORD(hit);
            if (HIWORD(hit) == 0 && idx >= 0 && idx < (int)gSessions.size()) {
                gRightClickedSessionPath = gSessions[idx].path;
                HMENU menu = CreatePopupMenu();
                AppendMenuW(menu, MF_STRING, IDM_DELETE_SESSION, L"Delete conversation");
                TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
                DestroyMenu(menu);
            }
        }
        return 0;
    case WM_CTLCOLORSTATIC:
        if ((HWND)lp == gSidebar) { SetBkColor((HDC)wp, RGB(246,247,251)); return (INT_PTR)gPanelBrush; }
        break;
    case WM_CTLCOLOREDIT:
        if ((HWND)lp == gChat || (HWND)lp == gInput) { SetBkColor((HDC)wp, RGB(255,255,255)); SetTextColor((HDC)wp, RGB(32,33,36)); return (INT_PTR)gInputBrush; }
        break;
    case WM_CTLCOLORLISTBOX:
        SetBkColor((HDC)wp, RGB(246,247,251)); SetTextColor((HDC)wp, RGB(32,33,36)); return (INT_PTR)gPanelBrush;
    case WM_DESTROY:
        SaveCurrentSession(); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    gInst = hInstance;
    InitCommonControls();
    LoadConfig();
    gFont = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    gBgBrush = CreateSolidBrush(RGB(255,255,255));
    gPanelBrush = CreateSolidBrush(RGB(246,247,251));
    gInputBrush = CreateSolidBrush(RGB(255,255,255));
    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"GeaiWindow";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = gBgBrush;
    RegisterClassW(&wc);
    gMain = CreateWindowW(L"GeaiWindow", L"Geai - Gemini Windows Client", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1040, 700, nullptr, nullptr, hInstance, nullptr);
    ShowWindow(gMain, nCmdShow);
    UpdateWindow(gMain);
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    DeleteObject(gFont); DeleteObject(gBgBrush); DeleteObject(gPanelBrush); DeleteObject(gInputBrush);
    return 0;
}
