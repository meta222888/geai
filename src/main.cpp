#include <windows.h>
#include <winhttp.h>
#include <commctrl.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <ctime>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "comctl32.lib")

#define IDC_CHAT 1001
#define IDC_INPUT 1002
#define IDC_SEND 1003
#define IDC_NEW 1004
#define IDC_SETTINGS 1005
#define IDC_SAVE 1006
#define IDC_LOAD 1007

struct Config {
    std::wstring apiKey;
    std::wstring apiBase = L"https://generativelanguage.googleapis.com";
    std::wstring model = L"gemini-2.5-flash";
    std::wstring proxy;
};

struct Message {
    std::wstring role;
    std::wstring text;
};

static HINSTANCE gInst;
static HWND gMain, gChat, gInput;
static Config gConfig;
static std::vector<Message> gMessages;

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

void AppendChat(const std::wstring& role, const std::wstring& text) {
    int len = GetWindowTextLengthW(gChat);
    SendMessageW(gChat, EM_SETSEL, len, len);
    std::wstring line = L"\r\n[" + role + L"]\r\n" + text + L"\r\n";
    SendMessageW(gChat, EM_REPLACESEL, FALSE, (LPARAM)line.c_str());
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

std::wstring ExtractText(const std::string& json) {
    std::string key = "\"text\":\"";
    auto pos = json.find(key);
    if (pos == std::string::npos) return Utf8ToWide(json);
    pos += key.size();
    std::string out;
    bool esc = false;
    for (; pos < json.size(); ++pos) {
        char c = json[pos];
        if (esc) {
            if (c == 'n') out += '\n';
            else if (c == 't') out += '\t';
            else out += c;
            esc = false;
        } else if (c == '\\') esc = true;
        else if (c == '"') break;
        else out += c;
    }
    return Utf8ToWide(out);
}

std::wstring CallGemini(const std::wstring& prompt) {
    std::wstring base = gConfig.apiBase;
    if (!base.empty() && base.back() == L'/') base.pop_back();
    std::wstring url = base + L"/v1beta/models/" + gConfig.model + L":generateContent";
    if (base.find(L"generativelanguage.googleapis.com") != std::wstring::npos) {
        url += L"?key=" + gConfig.apiKey;
    }

    URL_COMPONENTS uc{};
    std::wstring host, path;
    if (!ParseUrl(url, uc, host, path)) return L"Invalid API Base URL.";
    if (uc.lpszExtraInfo) path += uc.lpszExtraInfo;

    DWORD access = gConfig.proxy.empty() ? WINHTTP_ACCESS_TYPE_DEFAULT_PROXY : WINHTTP_ACCESS_TYPE_NAMED_PROXY;
    HINTERNET hSession = WinHttpOpen(L"Geai/0.1", access, gConfig.proxy.empty() ? WINHTTP_NO_PROXY_NAME : gConfig.proxy.c_str(), WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return L"WinHttpOpen failed.";

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), uc.nPort, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return L"WinHttpConnect failed."; }

    DWORD flags = uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return L"WinHttpOpenRequest failed."; }

    std::string body = "{\"contents\":[{\"role\":\"user\",\"parts\":[{\"text\":\"" + JsonEscape(WideToUtf8(prompt)) + "\"}]}]}";
    std::wstring headers = L"Content-Type: application/json\r\n";
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

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    if (!ok) return L"Request failed.";
    return ExtractText(response);
}

void SaveSession() {
    std::time_t t = std::time(nullptr);
    std::wstring path = AppDir() + L"\\sessions\\session_" + std::to_wstring((long long)t) + L".jsonl";
    std::ofstream f(path, std::ios::binary);
    for (auto& m : gMessages) {
        f << "{\"role\":\"" << WideToUtf8(m.role) << "\",\"text\":\"" << JsonEscape(WideToUtf8(m.text)) << "\"}\n";
    }
    MessageBoxW(gMain, path.c_str(), L"Session saved", MB_OK);
}

void NewSession() {
    gMessages.clear();
    SetWindowTextW(gChat, L"");
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
    int len = GetWindowTextLengthW(gInput);
    if (len <= 0) return;
    std::wstring prompt(len, 0);
    GetWindowTextW(gInput, prompt.data(), len + 1);
    SetWindowTextW(gInput, L"");
    gMessages.push_back({ L"user", prompt });
    AppendChat(L"user", prompt);
    AppendChat(L"system", L"Thinking...");
    std::wstring answer = CallGemini(prompt);
    gMessages.push_back({ L"assistant", answer });
    AppendChat(L"assistant", answer);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        gChat = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, 8, 8, 760, 420, hwnd, (HMENU)IDC_CHAT, gInst, nullptr);
        gInput = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL, 8, 438, 620, 70, hwnd, (HMENU)IDC_INPUT, gInst, nullptr);
        CreateWindowW(L"BUTTON", L"Send", WS_CHILD | WS_VISIBLE, 638, 438, 130, 32, hwnd, (HMENU)IDC_SEND, gInst, nullptr);
        CreateWindowW(L"BUTTON", L"New", WS_CHILD | WS_VISIBLE, 638, 476, 60, 32, hwnd, (HMENU)IDC_NEW, gInst, nullptr);
        CreateWindowW(L"BUTTON", L"Settings", WS_CHILD | WS_VISIBLE, 708, 476, 60, 32, hwnd, (HMENU)IDC_SETTINGS, gInst, nullptr);
        CreateWindowW(L"BUTTON", L"Save", WS_CHILD | WS_VISIBLE, 8, 516, 80, 28, hwnd, (HMENU)IDC_SAVE, gInst, nullptr);
        return 0;
    case WM_SIZE: {
        int w = LOWORD(lp), h = HIWORD(lp);
        MoveWindow(gChat, 8, 8, w - 16, h - 132, TRUE);
        MoveWindow(gInput, 8, h - 116, w - 156, 70, TRUE);
        MoveWindow(GetDlgItem(hwnd, IDC_SEND), w - 138, h - 116, 130, 32, TRUE);
        MoveWindow(GetDlgItem(hwnd, IDC_NEW), w - 138, h - 78, 60, 32, TRUE);
        MoveWindow(GetDlgItem(hwnd, IDC_SETTINGS), w - 68, h - 78, 60, 32, TRUE);
        MoveWindow(GetDlgItem(hwnd, IDC_SAVE), 8, h - 38, 80, 28, TRUE);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == IDC_SEND) SendPrompt();
        else if (LOWORD(wp) == IDC_NEW) NewSession();
        else if (LOWORD(wp) == IDC_SETTINGS) DialogBoxW(gInst, MAKEINTRESOURCEW(101), hwnd, SettingsProc);
        else if (LOWORD(wp) == IDC_SAVE) SaveSession();
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    gInst = hInstance;
    InitCommonControls();
    LoadConfig();

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"GeaiWindow";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&wc);

    gMain = CreateWindowW(L"GeaiWindow", L"Geai - Gemini Windows Client", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 820, 620, nullptr, nullptr, hInstance, nullptr);
    ShowWindow(gMain, nCmdShow);
    UpdateWindow(gMain);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
