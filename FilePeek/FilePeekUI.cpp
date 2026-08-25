#include <windows.h>
#include <wrl.h>
#include <string>

#include "WebView2.h"
#include "ExplorerDetector.h"

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;


// Window handles
HWND hiddenWindow = nullptr;
HWND triggerWindow = nullptr;
HWND menuWindow = nullptr;


// WebView state
ComPtr<ICoreWebView2Controller> controller;
ComPtr<ICoreWebView2> webview;


// Current Explorer item
RECT lastFileBounds = {};

bool haveFile = false;
bool menuOpen = false;

ULONGLONG lastFileSeenTime = 0;

std::wstring hoveredFileName;


// Check whether the cursor is inside one of our windows
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


// Position the trigger next to the hovered row
void ShowTrigger(
    const RECT& fileBounds)
{
    const int size = 24;
    const int gap = 6;

    int x =
        fileBounds.right + gap;

    int y =
        fileBounds.top +
        ((fileBounds.bottom - fileBounds.top) / 2) -
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


// Position the menu next to the trigger
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
    const int gap = 4;


    int x =
        triggerBounds.right + gap;

    int y =
        triggerBounds.top - 45;


    int screenWidth =
        GetSystemMetrics(
            SM_CXSCREEN
        );

    int screenHeight =
        GetSystemMetrics(
            SM_CYSCREEN
        );


    if (x + width > screenWidth)
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


    if (y + height > screenHeight)
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


// Reset the current hover state
void HideFilePeek()
{
    HideTrigger();

    HideMenu();


    haveFile = false;

    hoveredFileName.clear();
}


// Draw the green trigger
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


// Keep WebView sized to the host window
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


// Poll Explorer and update the FilePeek UI
LRESULT CALLBACK HiddenProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (message)
    {
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


        // Open the menu when the cursor reaches the trigger
        if (onTrigger)
        {
            if (!menuOpen)
            {
                ShowMenu();
            }


            return 0;
        }


        // Keep the menu open while the cursor is inside it
        if (onMenu)
        {
            return 0;
        }


        ExplorerItemInfo item =
            GetHoveredExplorerItem();


        if (item.found)
        {
            lastFileBounds =
                item.bounds;


            hoveredFileName =
                item.name;


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


        // Give the cursor time to move from the row to the trigger
        if (haveFile)
        {
            ULONGLONG now =
                GetTickCount64();


            ULONGLONG elapsed =
                now -
                lastFileSeenTime;


            if (elapsed < 450)
            {
                return 0;
            }
        }


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
        if (SUCCEEDED(comResult))
        {
            CoUninitialize();
        }


        return 0;
    }


    // Make the trigger circular
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


    // Initialize WebView2
    CreateCoreWebView2EnvironmentWithOptions(
        nullptr,
        nullptr,
        nullptr,

        Callback<
        ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(

            [](HRESULT result,
                ICoreWebView2Environment* environment)
            -> HRESULT
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
                        ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(

                            [](HRESULT result,
                                ICoreWebView2Controller* createdController)
                            -> HRESULT
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


                                // Keep the WebView transparent outside the cards
                                ComPtr<ICoreWebView2Controller2>
                                    controller2;


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

    background:
        transparent;

    overflow:
        hidden;
}


/* Menu */

.filepeek {

    position: absolute;

    left: 4px;

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


/* Menu options */

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


/* Arrow */

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


/* Summary preview */

.bubble {

    position: absolute;

    left:
        calc(100% + 18px);

    top: 50%;

    width: 340px;

    padding:
        18px
        20px;

    background:
        white;

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
        translateX(-4px);

    transition:
        opacity 0.14s ease,
        transform 0.14s ease,
        visibility 0.14s;

    z-index: 100;
}


/* Preview pointer */

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


.option:hover .bubble {

    opacity: 1;

    visibility: visible;

    transform:
        translateY(-50%)
        translateX(0);
}


/* Preview title */

.bubble-title {

    font-size: 15px;

    font-weight: 500;

    color: #3F4542;

    margin-bottom: 10px;
}


/* Preview text */

.bubble-text {

    font-size: 14px;

    font-weight: 400;

    line-height: 1.6;

    color:
        var(--soft-text);
}

</style>

</head>


<body>


<div class="filepeek">


    <div class="option">

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


            <div class="bubble-text">

                This is where the real quick
                summary will appear.

            </div>

        </div>

    </div>


    <div class="option">

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


            <div class="bubble-text">

                This is where the real detailed
                summary will appear.

            </div>

        </div>

    </div>


</div>


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


    // Poll Explorer often enough to keep the hover transition smooth
    SetTimer(
        hiddenWindow,
        1,
        50,
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


    if (SUCCEEDED(comResult))
    {
        CoUninitialize();
    }


    return 0;
}