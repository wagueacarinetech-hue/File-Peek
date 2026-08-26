#include <windows.h>
#include <wrl.h>

#include <string>
#include <thread>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "WebView2.h"
#include "ExplorerDetector.h"

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;


HWND hiddenWindow = nullptr;
HWND triggerWindow = nullptr;
HWND menuWindow = nullptr;

ComPtr<ICoreWebView2Controller> controller;
ComPtr<ICoreWebView2> webview;

RECT lastFileBounds = {};

bool haveFile = false;
bool menuOpen = false;

ULONGLONG lastFileSeenTime = 0;

std::wstring hoveredFileName;
std::wstring hoveredFilePath;


std::unordered_map<
    std::wstring,
    std::wstring
> summaryCache;


std::unordered_set<
    std::wstring
> pendingSummaries;


constexpr UINT WM_FILEPEEK_SUMMARY =
WM_APP + 1;


struct SummaryResult
{
    std::wstring path;
    std::wstring mode;
    std::wstring cacheKey;
    std::wstring text;

    bool success = false;
};


void LogPerformance(
    const std::wstring& label,
    ULONGLONG milliseconds)
{
    std::wstring message =
        L"[FilePeek Performance] " +
        label +
        L": " +
        std::to_wstring(milliseconds) +
        L" ms\n";

    OutputDebugStringW(
        message.c_str()
    );
}


bool MouseInsideWindow(HWND window)
{
    if (!window ||
        !IsWindowVisible(window))
    {
        return false;
    }

    POINT mouse;

    if (!GetCursorPos(&mouse))
    {
        return false;
    }

    RECT bounds;

    if (!GetWindowRect(
        window,
        &bounds))
    {
        return false;
    }

    return PtInRect(
        &bounds,
        mouse
    );
}


std::wstring MakeCacheKey(
    const std::wstring& path,
    const std::wstring& mode)
{
    WIN32_FILE_ATTRIBUTE_DATA data = {};

    if (!GetFileAttributesExW(
        path.c_str(),
        GetFileExInfoStandard,
        &data))
    {
        return path +
            L"|" +
            mode;
    }

    ULARGE_INTEGER modified;

    modified.LowPart =
        data.ftLastWriteTime.dwLowDateTime;

    modified.HighPart =
        data.ftLastWriteTime.dwHighDateTime;

    return path +
        L"|" +
        std::to_wstring(
            modified.QuadPart
        ) +
        L"|" +
        mode;
}


std::wstring EscapeForJavaScript(
    const std::wstring& text)
{
    std::wstring result;

    for (wchar_t ch : text)
    {
        switch (ch)
        {
        case L'\\':
            result += L"\\\\";
            break;

        case L'\'':
            result += L"\\'";
            break;

        case L'\r':
            break;

        case L'\n':
            result += L"\\n";
            break;

        case L'\t':
            result += L"\\t";
            break;

        case 0x2028:
            result += L"\\u2028";
            break;

        case 0x2029:
            result += L"\\u2029";
            break;

        default:
            result += ch;
            break;
        }
    }

    return result;
}


std::wstring Utf8ToWide(
    const std::string& text)
{
    if (text.empty())
    {
        return L"";
    }

    int length =
        MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            text.data(),
            static_cast<int>(
                text.size()
                ),
            nullptr,
            0
        );

    if (length <= 0)
    {
        length =
            MultiByteToWideChar(
                CP_UTF8,
                0,
                text.data(),
                static_cast<int>(
                    text.size()
                    ),
                nullptr,
                0
            );
    }

    if (length <= 0)
    {
        return L"";
    }

    std::wstring result(
        length,
        L'\0'
    );

    MultiByteToWideChar(
        CP_UTF8,
        0,
        text.data(),
        static_cast<int>(
            text.size()
            ),
        result.data(),
        length
    );

    return result;
}


void ShowLoading(
    const std::wstring& mode)
{
    if (!webview)
    {
        return;
    }

    std::wstring script =
        L"setLoading('" +
        EscapeForJavaScript(mode) +
        L"');";

    webview->ExecuteScript(
        script.c_str(),
        nullptr
    );
}


void ShowSummary(
    const std::wstring& mode,
    const std::wstring& text,
    bool success)
{
    if (!webview)
    {
        return;
    }

    std::wstring script =
        L"showSummary('" +
        EscapeForJavaScript(mode) +
        L"', '" +
        EscapeForJavaScript(text) +
        L"', " +
        (
            success
            ? L"true"
            : L"false"
            ) +
        L");";

    webview->ExecuteScript(
        script.c_str(),
        nullptr
    );
}


std::wstring GetExecutableDirectory()
{
    wchar_t buffer[MAX_PATH] = {};

    DWORD length = GetModuleFileNameW(
        nullptr,
        buffer,
        MAX_PATH
    );

    if (length == 0 || length >= MAX_PATH)
    {
        return L"";
    }

    std::wstring path(buffer, length);

    size_t lastSlash = path.find_last_of(L"\\/");

    if (lastSlash == std::wstring::npos)
    {
        return L"";
    }

    return path.substr(0, lastSlash);
}


std::wstring GetBackendPath()
{
    std::wstring directory = GetExecutableDirectory();

    if (directory.empty())
    {
        return L"FilePeekBackend.exe";
    }

    return directory + L"\\FilePeekBackend.exe";
}


SummaryResult RunBackend(
    const std::wstring& filePath,
    const std::wstring& mode,
    const std::wstring& cacheKey)
{
    ULONGLONG totalStart = GetTickCount64();

    SummaryResult result;
    result.path = filePath;
    result.mode = mode;
    result.cacheKey = cacheKey;

    const std::wstring backendPath = GetBackendPath();

    if (GetFileAttributesW(backendPath.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        result.text = L"FilePeekBackend.exe was not found next to FilePeek.exe.";

        LogPerformance(
            mode + L" backend failed",
            GetTickCount64() - totalStart
        );

        return result;
    }

    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;

    SECURITY_ATTRIBUTES security = {};
    security.nLength = sizeof(SECURITY_ATTRIBUTES);
    security.bInheritHandle = TRUE;

    if (!CreatePipe(
        &readPipe,
        &writePipe,
        &security,
        0))
    {
        result.text = L"Could not create the FilePeek backend connection.";

        LogPerformance(
            mode + L" backend failed",
            GetTickCount64() - totalStart
        );

        return result;
    }

    SetHandleInformation(
        readPipe,
        HANDLE_FLAG_INHERIT,
        0
    );

    STARTUPINFOW startup = {};
    startup.cb = sizeof(STARTUPINFOW);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = writePipe;
    startup.hStdError = writePipe;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION process = {};

    std::wstring command =
        L"\"" +
        backendPath +
        L"\" \"" +
        filePath +
        L"\" " +
        mode;

    std::vector<wchar_t> commandBuffer(
        command.begin(),
        command.end()
    );

    commandBuffer.push_back(L'\0');

    BOOL started = CreateProcessW(
        backendPath.c_str(),
        commandBuffer.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &startup,
        &process
    );

    CloseHandle(writePipe);

    if (!started)
    {
        CloseHandle(readPipe);

        result.text = L"FilePeek could not start FilePeekBackend.exe.";

        LogPerformance(
            mode + L" backend failed",
            GetTickCount64() - totalStart
        );

        return result;
    }

    std::string output;
    char buffer[4096];
    DWORD bytesRead = 0;

    while (ReadFile(
        readPipe,
        buffer,
        sizeof(buffer),
        &bytesRead,
        nullptr))
    {
        if (bytesRead == 0)
        {
            break;
        }

        output.append(buffer, bytesRead);
    }

    WaitForSingleObject(
        process.hProcess,
        INFINITE
    );

    DWORD exitCode = 1;

    GetExitCodeProcess(
        process.hProcess,
        &exitCode
    );

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    CloseHandle(readPipe);

    std::wstring converted = Utf8ToWide(output);

    while (!converted.empty() &&
        (converted.back() == L'\r' ||
            converted.back() == L'\n'))
    {
        converted.pop_back();
    }

    LogPerformance(
        mode + L" backend",
        GetTickCount64() - totalStart
    );

    if (converted.empty())
    {
        result.text = L"The backend returned no text.";
        return result;
    }

    if (exitCode != 0 ||
        converted.rfind(L"ERROR:", 0) == 0)
    {
        result.text = converted;
        return result;
    }

    result.text = converted;
    result.success = true;

    return result;
}


void StartSummary(
    const std::wstring& mode)
{
    ULONGLONG requestStart =
        GetTickCount64();

    if (hoveredFilePath.empty())
    {
        ShowSummary(
            mode,
            L"FilePeek could not resolve this file.",
            false
        );

        return;
    }

    std::wstring filePath =
        hoveredFilePath;

    std::wstring cacheKey =
        MakeCacheKey(
            filePath,
            mode
        );

    auto cached =
        summaryCache.find(
            cacheKey
        );

    if (cached !=
        summaryCache.end())
    {
        ShowSummary(
            mode,
            cached->second,
            true
        );

        LogPerformance(
            mode + L" cache hit",
            GetTickCount64() -
            requestStart
        );

        return;
    }

    if (pendingSummaries.find(
        cacheKey) !=
        pendingSummaries.end())
    {
        ShowLoading(
            mode
        );

        return;
    }

    pendingSummaries.insert(
        cacheKey
    );

    ShowLoading(
        mode
    );

    LogPerformance(
        mode + L" request dispatch",
        GetTickCount64() -
        requestStart
    );

    std::thread(
        [
            filePath,
            mode,
            cacheKey
        ]()
        {
            SummaryResult result =
                RunBackend(
                    filePath,
                    mode,
                    cacheKey
                );

            SummaryResult* heapResult =
                new SummaryResult(
                    std::move(result)
                );

            PostMessageW(
                hiddenWindow,
                WM_FILEPEEK_SUMMARY,
                0,
                reinterpret_cast<LPARAM>(
                    heapResult
                    )
            );
        }
    ).detach();
}


void ShowTrigger(
    const RECT& fileBounds)
{
    const int size = 24;
    const int gap = 6;

    int x =
        fileBounds.right +
        gap;

    int y =
        fileBounds.top +
        (
            (
                fileBounds.bottom -
                fileBounds.top
                ) / 2
            ) -
        (size / 2);

    SetWindowPos(
        triggerWindow,
        HWND_TOPMOST,
        x,
        y,
        size,
        size,
        SWP_SHOWWINDOW |
        SWP_NOACTIVATE
    );
}


void HideTrigger()
{
    ShowWindow(
        triggerWindow,
        SW_HIDE
    );
}


void ShowMenu()
{
    RECT triggerBounds;

    if (!GetWindowRect(
        triggerWindow,
        &triggerBounds))
    {
        return;
    }

    const int width = 640;
    const int height = 220;
    const int gap = 0;

    int x =
        triggerBounds.right +
        gap;

    int y =
        triggerBounds.top -
        45;

    int screenWidth =
        GetSystemMetrics(
            SM_CXSCREEN
        );

    int screenHeight =
        GetSystemMetrics(
            SM_CYSCREEN
        );

    if (x + width >
        screenWidth)
    {
        x =
            triggerBounds.left -
            width -
            gap;
    }

    if (y < 0)
    {
        y = 0;
    }

    if (y + height >
        screenHeight)
    {
        y =
            screenHeight -
            height;
    }

    SetWindowPos(
        menuWindow,
        HWND_TOPMOST,
        x,
        y,
        width,
        height,
        SWP_SHOWWINDOW |
        SWP_NOACTIVATE
    );

    menuOpen = true;

    if (controller)
    {
        RECT bounds;

        GetClientRect(
            menuWindow,
            &bounds
        );

        controller->put_Bounds(
            bounds
        );

        controller->put_IsVisible(
            TRUE
        );
    }
}


void HideMenu()
{
    if (controller)
    {
        controller->put_IsVisible(
            FALSE
        );
    }

    ShowWindow(
        menuWindow,
        SW_HIDE
    );

    menuOpen = false;
}


void HideFilePeek()
{
    HideTrigger();

    HideMenu();

    haveFile = false;

    lastFileSeenTime = 0;

    hoveredFileName.clear();

    hoveredFilePath.clear();
}


LRESULT CALLBACK TriggerProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (message)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;

        HDC hdc =
            BeginPaint(
                hwnd,
                &ps
            );

        RECT rect;

        GetClientRect(
            hwnd,
            &rect
        );

        HBRUSH brush =
            CreateSolidBrush(
                RGB(
                    34,
                    160,
                    107
                )
            );

        FillRect(
            hdc,
            &rect,
            brush
        );

        DeleteObject(
            brush
        );

        EndPaint(
            hwnd,
            &ps
        );

        return 0;
    }
    }

    return DefWindowProcW(
        hwnd,
        message,
        wParam,
        lParam
    );
}


LRESULT CALLBACK MenuProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (message)
    {
    case WM_SIZE:
    {
        if (controller)
        {
            RECT bounds;

            GetClientRect(
                hwnd,
                &bounds
            );

            controller->put_Bounds(
                bounds
            );
        }

        return 0;
    }

    case WM_ERASEBKGND:
    {
        return 1;
    }
    }

    return DefWindowProcW(
        hwnd,
        message,
        wParam,
        lParam
    );
}


LRESULT CALLBACK HiddenProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (message)
    {
    case WM_FILEPEEK_SUMMARY:
    {
        SummaryResult* result =
            reinterpret_cast<
            SummaryResult*
            >(
                lParam
                );

        if (!result)
        {
            return 0;
        }

        pendingSummaries.erase(
            result->cacheKey
        );

        if (result->success)
        {
            summaryCache[
                result->cacheKey
            ] =
                result->text;
        }

        if (!hoveredFilePath.empty() &&
            MakeCacheKey(
                hoveredFilePath,
                result->mode
            ) ==
            result->cacheKey)
        {
            ShowSummary(
                result->mode,
                result->text,
                result->success
            );
        }

        delete result;

        return 0;
    }


    case WM_TIMER:
    {
        bool onTrigger =
            MouseInsideWindow(
                triggerWindow
            );

        bool onMenu =
            MouseInsideWindow(
                menuWindow
            );

        if (onTrigger)
        {
            SetTimer(
                hiddenWindow,
                1,
                50,
                nullptr
            );

            if (!menuOpen)
            {
                ShowMenu();
            }

            return 0;
        }

        if (onMenu)
        {
            SetTimer(
                hiddenWindow,
                1,
                50,
                nullptr
            );

            return 0;
        }

        ExplorerItemInfo item =
            GetHoveredExplorerItem();

        if (item.found &&
            !item.path.empty())
        {
            SetTimer(
                hiddenWindow,
                1,
                50,
                nullptr
            );

            lastFileBounds =
                item.bounds;

            hoveredFileName =
                item.name;

            hoveredFilePath =
                item.path;

            lastFileSeenTime =
                GetTickCount64();

            haveFile = true;

            if (menuOpen)
            {
                HideMenu();
            }

            ShowTrigger(
                lastFileBounds
            );

            return 0;
        }

        if (haveFile)
        {
            ULONGLONG elapsed =
                GetTickCount64() -
                lastFileSeenTime;

            if (elapsed < 450)
            {
                SetTimer(
                    hiddenWindow,
                    1,
                    50,
                    nullptr
                );

                return 0;
            }
        }

        SetTimer(
            hiddenWindow,
            1,
            250,
            nullptr
        );

        HideFilePeek();

        return 0;
    }


    case WM_DESTROY:
    {
        KillTimer(
            hwnd,
            1
        );

        PostQuitMessage(
            0
        );

        return 0;
    }
    }

    return DefWindowProcW(
        hwnd,
        message,
        wParam,
        lParam
    );
}


int WINAPI wWinMain(
    _In_ HINSTANCE instance,
    _In_opt_ HINSTANCE previousInstance,
    _In_ PWSTR commandLine,
    _In_ int showCommand)
{
    HRESULT comResult =
        CoInitializeEx(
            nullptr,
            COINIT_APARTMENTTHREADED
        );


    const wchar_t HIDDEN_CLASS[] =
        L"FilePeekHidden";

    WNDCLASSW hiddenClass = {};

    hiddenClass.lpfnWndProc =
        HiddenProc;

    hiddenClass.hInstance =
        instance;

    hiddenClass.lpszClassName =
        HIDDEN_CLASS;

    RegisterClassW(
        &hiddenClass
    );


    const wchar_t TRIGGER_CLASS[] =
        L"FilePeekTrigger";

    WNDCLASSW triggerClass = {};

    triggerClass.lpfnWndProc =
        TriggerProc;

    triggerClass.hInstance =
        instance;

    triggerClass.lpszClassName =
        TRIGGER_CLASS;

    triggerClass.hCursor =
        LoadCursor(
            nullptr,
            IDC_HAND
        );

    RegisterClassW(
        &triggerClass
    );


    const wchar_t MENU_CLASS[] =
        L"FilePeekMenu";

    WNDCLASSW menuClass = {};

    menuClass.lpfnWndProc =
        MenuProc;

    menuClass.hInstance =
        instance;

    menuClass.lpszClassName =
        MENU_CLASS;

    menuClass.hCursor =
        LoadCursor(
            nullptr,
            IDC_ARROW
        );

    menuClass.hbrBackground =
        (HBRUSH)GetStockObject(
            NULL_BRUSH
        );

    RegisterClassW(
        &menuClass
    );


    hiddenWindow =
        CreateWindowExW(
            0,
            HIDDEN_CLASS,
            L"",
            0,
            0,
            0,
            0,
            0,
            nullptr,
            nullptr,
            instance,
            nullptr
        );


    triggerWindow =
        CreateWindowExW(
            WS_EX_TOPMOST |
            WS_EX_TOOLWINDOW |
            WS_EX_NOACTIVATE,
            TRIGGER_CLASS,
            L"",
            WS_POPUP,
            0,
            0,
            24,
            24,
            nullptr,
            nullptr,
            instance,
            nullptr
        );


    menuWindow =
        CreateWindowExW(
            WS_EX_TOPMOST |
            WS_EX_TOOLWINDOW |
            WS_EX_NOACTIVATE,
            MENU_CLASS,
            L"",
            WS_POPUP,
            0,
            0,
            640,
            220,
            nullptr,
            nullptr,
            instance,
            nullptr
        );


    if (!hiddenWindow ||
        !triggerWindow ||
        !menuWindow)
    {
        if (SUCCEEDED(
            comResult))
        {
            CoUninitialize();
        }

        return 0;
    }


    HRGN circle =
        CreateEllipticRgn(
            0,
            0,
            24,
            24
        );

    SetWindowRgn(
        triggerWindow,
        circle,
        TRUE
    );

    ShowWindow(
        triggerWindow,
        SW_HIDE
    );

    ShowWindow(
        menuWindow,
        SW_HIDE
    );


    CreateCoreWebView2EnvironmentWithOptions(
        nullptr,
        nullptr,
        nullptr,

        Callback<
        ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler
        >(

            [](
                HRESULT result,
                ICoreWebView2Environment* environment
                ) -> HRESULT
            {
                if (FAILED(result) ||
                    !environment)
                {
                    return E_FAIL;
                }


                environment->
                    CreateCoreWebView2Controller(
                        menuWindow,

                        Callback<
                        ICoreWebView2CreateCoreWebView2ControllerCompletedHandler
                        >(

                            [](
                                HRESULT result,
                                ICoreWebView2Controller* createdController
                                ) -> HRESULT
                            {
                                if (FAILED(result) ||
                                    !createdController)
                                {
                                    return E_FAIL;
                                }


                                controller =
                                    createdController;

                                createdController->
                                    get_CoreWebView2(
                                        &webview
                                    );


                                RECT bounds;

                                GetClientRect(
                                    menuWindow,
                                    &bounds
                                );

                                controller->
                                    put_Bounds(
                                        bounds
                                    );

                                controller->
                                    put_IsVisible(
                                        FALSE
                                    );


                                ComPtr<
                                    ICoreWebView2Controller2
                                > controller2;


                                if (SUCCEEDED(
                                    controller.As(
                                        &controller2
                                    )))
                                {
                                    COREWEBVIEW2_COLOR transparentColor =
                                    {
                                        0,
                                        0,
                                        0,
                                        0
                                    };

                                    controller2->
                                        put_DefaultBackgroundColor(
                                            transparentColor
                                        );
                                }


                                EventRegistrationToken token;


                                webview->
                                    add_WebMessageReceived(

                                        Callback<
                                        ICoreWebView2WebMessageReceivedEventHandler
                                        >(

                                            [](
                                                ICoreWebView2* sender,
                                                ICoreWebView2WebMessageReceivedEventArgs* args
                                                ) -> HRESULT
                                            {
                                                LPWSTR message =
                                                    nullptr;

                                                HRESULT result =
                                                    args->
                                                    TryGetWebMessageAsString(
                                                        &message
                                                    );

                                                if (FAILED(result) ||
                                                    !message)
                                                {
                                                    return S_OK;
                                                }

                                                std::wstring action =
                                                    message;

                                                CoTaskMemFree(
                                                    message
                                                );

                                                if (action ==
                                                    L"quick")
                                                {
                                                    StartSummary(
                                                        L"quick"
                                                    );
                                                }
                                                else if (
                                                    action ==
                                                    L"detailed")
                                                {
                                                    StartSummary(
                                                        L"detailed"
                                                    );
                                                }

                                                return S_OK;
                                            }

                                        ).Get(),

                                        &token
                                    );


                                const wchar_t* html =
                                    LR"FILEPEEK(

<!DOCTYPE html>

<html>

<head>

<meta charset="UTF-8">

<style>

* {
    box-sizing: border-box;
}


:root {
    --green: #22A06B;
    --green-hover: #2CAF78;
    --main-text: #303633;
    --soft-text: #4F5552;
    --error-text: #A33A3A;
}


html,
body {
    margin: 0;
    padding: 0;

    width: 100%;
    height: 100%;

    font-family:
        "Segoe UI",
        Arial,
        sans-serif;

    background: transparent;

    overflow: hidden;
}


.filepeek {
    position: absolute;

    left: 0;
    top: 45px;

    width: 245px;

    padding: 10px;

    background:
        var(--green);

    border-radius: 16px;

    box-shadow:
        0 6px 18px
        rgba(0, 0, 0, 0.12);
}


.option {
    position: relative;

    padding:
        12px
        13px;

    border-radius: 10px;

    font-size: 14px;
    font-weight: 500;

    color: white;

    cursor: default;

    transition:
        background 0.14s ease;
}


.option + .option {
    margin-top: 3px;
}


.option:hover {
    background:
        var(--green-hover);
}


.option::after {
    content: "";

    position: absolute;

    top: -4px;
    bottom: -4px;

    right: -22px;

    width: 24px;

    background: transparent;
}


.option-label {
    display: flex;

    align-items: center;

    justify-content: space-between;
}


.arrow {
    font-size: 17px;
    font-weight: 500;

    color:
        rgba(
            255,
            255,
            255,
            0.82
        );
}


.bubble {
    position: absolute;

    left:
        calc(100% + 10px);

    top: 50%;

    width: 340px;

    max-height: 190px;

    padding:
        18px
        20px;

    background: white;

    color:
        var(--main-text);

    border-radius: 18px;

    border:
        1px solid
        rgba(0, 0, 0, 0.05);

    box-shadow:
        0 7px 20px
        rgba(0, 0, 0, 0.08);

    opacity: 0;

    visibility: hidden;

    transform:
        translateY(-50%)
        translateX(-3px);

    transition:
        opacity 0.12s ease,
        transform 0.12s ease,
        visibility 0.12s;

    z-index: 100;

    overflow-y: auto;

    overscroll-behavior:
        contain;
}


.bubble::before {
    content: "";

    position: absolute;

    left: -10px;
    top: 50%;

    transform:
        translateY(-50%);

    width: 0;
    height: 0;

    border-top:
        10px solid transparent;

    border-bottom:
        10px solid transparent;

    border-right:
        11px solid white;
}


.option:hover .bubble,
.bubble:hover {
    opacity: 1;

    visibility: visible;

    transform:
        translateY(-50%)
        translateX(0);
}


.bubble-title {
    position: sticky;

    top: 0;

    padding-bottom: 8px;

    background: white;

    font-size: 15px;

    font-weight: 600;

    color: #303633;

    margin-bottom: 5px;

    z-index: 2;
}


.bubble-text {
    font-size: 14px;

    font-weight: 400;

    line-height: 1.5;

    color:
        var(--soft-text);
}


.bubble-text strong {
    font-weight: 600;

    color:
        var(--main-text);
}


.bubble-text h3 {
    margin:
        11px
        0
        5px
        0;

    font-size: 14px;

    font-weight: 600;

    color:
        var(--main-text);
}


.summary-line {
    margin:
        3px
        0;
}


.summary-item {
    position: relative;

    margin:
        5px
        0;

    padding-left: 15px;
}


.summary-item::before {
    content: "-";

    position: absolute;

    left: 1px;

    color:
        var(--green);
}


.summary-number {
    margin:
        5px
        0;
}


.summary-space {
    height: 5px;
}


.bubble-text.error {
    color:
        var(--error-text);
}

</style>

</head>


<body>


<div class="filepeek">


    <div
        class="option"
        id="quickOption">

        <div class="option-label">

            <span>
                Quick Summary
            </span>

            <span class="arrow">
                &gt;
            </span>

        </div>


        <div class="bubble">

            <div class="bubble-title">
                Quick Summary
            </div>


            <div
                class="bubble-text"
                id="quickText">

                Hover here to identify this file.

            </div>

        </div>

    </div>


    <div
        class="option"
        id="detailedOption">

        <div class="option-label">

            <span>
                Detailed Summary
            </span>

            <span class="arrow">
                &gt;
            </span>

        </div>


        <div class="bubble">

            <div class="bubble-title">
                Detailed Summary
            </div>


            <div
                class="bubble-text"
                id="detailedText">

                Hover here to understand this file.

            </div>

        </div>

    </div>


</div>


<script>

const requestTimers = {
    quick: null,
    detailed: null
};


function getTextElement(mode)
{
    if (mode === "quick")
    {
        return document.getElementById(
            "quickText"
        );
    }

    return document.getElementById(
        "detailedText"
    );
}


function cleanEncoding(text)
{
    if (!text)
    {
        return "";
    }

    return text
        .replace(/â€”/g, "-")
        .replace(/â€“/g, "-")
        .replace(/â€™/g, "'")
        .replace(/â€˜/g, "'")
        .replace(/â€œ/g, "\"")
        .replace(/â€/g, "\"")
        .replace(/â€¢/g, "-")
        .replace(/â€˘/g, "-")
        .replace(/Â /g, " ")
        .replace(/Â/g, "")
        .replace(/ï¿½/g, "")
        .replace(/\uFFFD/g, "")
        .replace(/[\u2013\u2014]/g, "-")
        .replace(/[\u2018\u2019]/g, "'")
        .replace(/[\u201C\u201D]/g, "\"")
        .replace(/\u2022/g, "-");
}


function formatSummary(text)
{
    text =
        cleanEncoding(text);

    let safe = text
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;");

    const lines =
        safe.split(/\r?\n/);

    let html = "";

    for (let line of lines)
    {
        line = line.trim();

        if (line.length === 0)
        {
            html +=
                "<div class='summary-space'></div>";

            continue;
        }


        line = line.replace(
            /\*\*(.*?)\*\*/g,
            "<strong>$1</strong>"
        );


        if (/^#{1,3}\s+/.test(line))
        {
            line = line.replace(
                /^#{1,3}\s+/,
                ""
            );

            html +=
                "<h3>" +
                line +
                "</h3>";

            continue;
        }


        if (/^[-*]\s+/.test(line))
        {
            line = line.replace(
                /^[-*]\s+/,
                ""
            );

            html +=
                "<div class='summary-item'>" +
                line +
                "</div>";

            continue;
        }


        if (/^\d+\.\s+/.test(line))
        {
            html +=
                "<div class='summary-number'>" +
                line +
                "</div>";

            continue;
        }


        html +=
            "<div class='summary-line'>" +
            line +
            "</div>";
    }

    return html;
}


function setLoading(mode)
{
    const element =
        getTextElement(mode);

    element.classList.remove(
        "error"
    );

    element.textContent =
        "Summarizing...";
}


function showSummary(
    mode,
    text,
    success)
{
    const element =
        getTextElement(mode);


    if (success)
    {
        element.classList.remove(
            "error"
        );

        element.innerHTML =
            formatSummary(text);
    }
    else
    {
        element.classList.add(
            "error"
        );

        element.textContent =
            cleanEncoding(text);
    }
}


function startHoverRequest(mode)
{
    clearTimeout(
        requestTimers[mode]
    );

    requestTimers[mode] =
        setTimeout(
            function()
            {
                window.chrome.webview.postMessage(
                    mode
                );
            },
            220
        );
}


function cancelHoverRequest(mode)
{
    if (requestTimers[mode])
    {
        clearTimeout(
            requestTimers[mode]
        );

        requestTimers[mode] =
            null;
    }
}


const quick =
    document.getElementById(
        "quickOption"
    );


const detailed =
    document.getElementById(
        "detailedOption"
    );


quick.addEventListener(
    "mouseenter",
    function()
    {
        startHoverRequest(
            "quick"
        );
    }
);


quick.addEventListener(
    "mouseleave",
    function()
    {
        cancelHoverRequest(
            "quick"
        );
    }
);


detailed.addEventListener(
    "mouseenter",
    function()
    {
        startHoverRequest(
            "detailed"
        );
    }
);


detailed.addEventListener(
    "mouseleave",
    function()
    {
        cancelHoverRequest(
            "detailed"
        );
    }
);

</script>


</body>

</html>

)FILEPEEK";


                                webview->
                                    NavigateToString(
                                        html
                                    );

                                return S_OK;
                            }

                        ).Get()
                    );

                return S_OK;
            }

        ).Get()
    );


    SetTimer(
        hiddenWindow,
        1,
        250,
        nullptr
    );


    MSG message = {};


    while (GetMessageW(
        &message,
        nullptr,
        0,
        0))
    {
        TranslateMessage(
            &message
        );

        DispatchMessageW(
            &message
        );
    }


    if (SUCCEEDED(
        comResult))
    {
        CoUninitialize();
    }

    return 0;
}