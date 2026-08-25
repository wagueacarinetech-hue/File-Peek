#include <windows.h>
#include <wrl.h>
#include "WebView2.h"

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

HWND mainWindow = nullptr;

ComPtr<ICoreWebView2Controller> controller;
ComPtr<ICoreWebView2> webview;


// resize webview with window
LRESULT CALLBACK WindowProc(
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
            GetClientRect(hwnd, &bounds);

            controller->put_Bounds(bounds);
        }

        return 0;
    }

    case WM_DESTROY:
    {
        PostQuitMessage(0);
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
    HINSTANCE instance,
    HINSTANCE,
    PWSTR,
    int showCommand)
{
    const wchar_t CLASS_NAME[] =
        L"FilePeekWebView";


    WNDCLASSW windowClass = {};

    windowClass.lpfnWndProc =
        WindowProc;

    windowClass.hInstance =
        instance;

    windowClass.lpszClassName =
        CLASS_NAME;

    windowClass.hCursor =
        LoadCursor(nullptr, IDC_ARROW);

    windowClass.hbrBackground =
        (HBRUSH)(COLOR_WINDOW + 1);


    if (!RegisterClassW(&windowClass))
    {
        return 0;
    }


    mainWindow = CreateWindowExW(
        WS_EX_TOPMOST |
        WS_EX_TOOLWINDOW,

        CLASS_NAME,

        L"FilePeek",

        WS_OVERLAPPEDWINDOW,

        350,
        250,

        720,
        420,

        nullptr,
        nullptr,
        instance,
        nullptr
    );


    if (!mainWindow)
    {
        return 0;
    }


    ShowWindow(
        mainWindow,
        showCommand
    );

    UpdateWindow(mainWindow);


    // start webview
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


                environment->CreateCoreWebView2Controller(
                    mainWindow,

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


                            createdController->get_CoreWebView2(
                                &webview
                            );


                            RECT bounds;

                            GetClientRect(
                                mainWindow,
                                &bounds
                            );


                            controller->put_Bounds(
                                bounds
                            );


                            // filepeek interface
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

    --bubble-background: #FFFFFF;

    --main-text: #303633;

    --soft-text: #4F5552;
}


body {

    margin: 0;

    padding: 35px;

    font-family:
        "Segoe UI",
        Arial,
        sans-serif;

    background: #F5F6F6;

    overflow: hidden;
}


/* green first panel */

.filepeek {

    width: 245px;

    padding: 10px;

    background:
        var(--green);

    border-radius: 16px;

    box-shadow:
        0 6px 18px
        rgba(0, 0, 0, 0.12);
}


/* summary choices */

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


/* lighter green hover */

.option:hover {

    background:
        var(--green-hover);
}


/* text and arrow */

.option-label {

    display: flex;

    align-items: center;

    justify-content: space-between;
}


.arrow {

    font-size: 18px;

    font-weight: 600;

    color:
        rgba(255, 255, 255, 0.82);

    transition:
        transform 0.14s ease,
        color 0.14s ease;
}


.option:hover .arrow {

    color: white;

    transform:
        translateX(2px);
}


/* calm reading bubble */

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
        var(--bubble-background);

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


/* message tail */

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


/* show message */

.option:hover .bubble {

    opacity: 1;

    visibility: visible;

    transform:
        translateY(-50%)
        translateX(0);
}


/* calm heading */

.bubble-title {

    font-size: 15px;

    font-weight: 500;

    color: #3F4542;

    margin-bottom: 10px;
}


/* easier reading */

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


    <!-- quick summary -->

    <div class="option">

        <div class="option-label">

            <span>
                Quick Summary
            </span>

            <span class="arrow">
                ›
            </span>

        </div>


        <div class="bubble">

            <div class="bubble-title">
                Quick Summary
            </div>


            <div class="bubble-text">

                This file discusses hashing,
                tries, and algorithm analysis.

            </div>

        </div>

    </div>


    <!-- detailed summary -->

    <div class="option">

        <div class="option-label">

            <span>
                Detailed Summary
            </span>

            <span class="arrow">
                ›
            </span>

        </div>


        <div class="bubble">

            <div class="bubble-title">
                Detailed Summary
            </div>


            <div class="bubble-text">

                This file covers hash-table probing,
                modular pairing, trie-based word search,
                proofs of correctness, and runtime analysis.

            </div>

        </div>

    </div>


</div>


</body>

</html>

)FILEPEEK";


                            webview->NavigateToString(
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


    // message loop
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


    return 0;
}