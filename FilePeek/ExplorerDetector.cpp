#include "ExplorerDetector.h"

#include <UIAutomation.h>
#include <exdisp.h>
#include <shlwapi.h>
#include <filesystem>

#pragma comment(lib, "uiautomationcore.lib")
#pragma comment(lib, "shlwapi.lib")


std::wstring GetElementName(
    IUIAutomationElement* element)
{
    if (!element)
    {
        return L"";
    }

    BSTR name = nullptr;

    HRESULT result =
        element->get_CurrentName(&name);

    if (FAILED(result) || !name)
    {
        return L"";
    }

    std::wstring value = name;

    SysFreeString(name);

    return value;
}


std::wstring GetExplorerFolderPath(
    HWND explorerWindow)
{
    IShellWindows* shellWindows = nullptr;

    HRESULT result =
        CoCreateInstance(
            CLSID_ShellWindows,
            nullptr,
            CLSCTX_ALL,
            IID_PPV_ARGS(&shellWindows)
        );

    if (FAILED(result) || !shellWindows)
    {
        return L"";
    }

    long count = 0;

    shellWindows->get_Count(&count);

    std::wstring folderPath;


    for (long i = 0; i < count; i++)
    {
        VARIANT index;

        VariantInit(&index);

        index.vt = VT_I4;
        index.lVal = i;


        IDispatch* dispatch = nullptr;

        shellWindows->Item(
            index,
            &dispatch
        );

        VariantClear(&index);


        if (!dispatch)
        {
            continue;
        }


        IWebBrowserApp* browser = nullptr;

        result =
            dispatch->QueryInterface(
                IID_PPV_ARGS(&browser)
            );

        dispatch->Release();


        if (FAILED(result) || !browser)
        {
            continue;
        }


        SHANDLE_PTR browserHandle = 0;

        browser->get_HWND(
            &browserHandle
        );


        HWND browserWindow =
            reinterpret_cast<HWND>(
                browserHandle
                );

        browserWindow =
            GetAncestor(
                browserWindow,
                GA_ROOT
            );


        if (browserWindow == explorerWindow)
        {
            BSTR locationUrl = nullptr;

            browser->get_LocationURL(
                &locationUrl
            );


            if (locationUrl)
            {
                wchar_t pathBuffer[32768];

                DWORD pathLength = 32768;


                HRESULT pathResult =
                    PathCreateFromUrlW(
                        locationUrl,
                        pathBuffer,
                        &pathLength,
                        0
                    );


                if (SUCCEEDED(pathResult))
                {
                    folderPath =
                        pathBuffer;
                }


                SysFreeString(
                    locationUrl
                );
            }


            browser->Release();

            break;
        }


        browser->Release();
    }


    shellWindows->Release();

    return folderPath;
}


std::wstring ResolveItemPath(
    const std::wstring& folder,
    const std::wstring& displayedName)
{
    if (folder.empty() ||
        displayedName.empty())
    {
        return L"";
    }


    namespace fs = std::filesystem;

    fs::path folderPath(folder);

    fs::path directPath =
        folderPath /
        displayedName;


    std::error_code error;


    if (fs::exists(
        directPath,
        error))
    {
        return directPath.wstring();
    }


    for (const auto& entry :
        fs::directory_iterator(
            folderPath,
            error))
    {
        if (error)
        {
            break;
        }


        std::wstring filename =
            entry.path()
            .filename()
            .wstring();


        std::wstring stem =
            entry.path()
            .stem()
            .wstring();


        if (_wcsicmp(
            filename.c_str(),
            displayedName.c_str()) == 0 ||
            _wcsicmp(
                stem.c_str(),
                displayedName.c_str()) == 0)
        {
            return entry.path()
                .wstring();
        }
    }


    return L"";
}


ExplorerItemInfo GetHoveredExplorerItem()
{
    ExplorerItemInfo info;


    POINT mouse;


    if (!GetCursorPos(&mouse))
    {
        return info;
    }


    HWND pointWindow =
        WindowFromPoint(mouse);


    HWND explorerWindow =
        GetAncestor(
            pointWindow,
            GA_ROOT
        );


    IUIAutomation* automation = nullptr;


    HRESULT result =
        CoCreateInstance(
            CLSID_CUIAutomation,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&automation)
        );


    if (FAILED(result) || !automation)
    {
        return info;
    }


    IUIAutomationElement* element = nullptr;


    result =
        automation->ElementFromPoint(
            mouse,
            &element
        );


    if (FAILED(result) || !element)
    {
        automation->Release();

        return info;
    }


    IUIAutomationTreeWalker* walker = nullptr;


    result =
        automation->get_ControlViewWalker(
            &walker
        );


    if (FAILED(result) || !walker)
    {
        element->Release();
        automation->Release();

        return info;
    }


    IUIAutomationElement* current =
        element;

    current->AddRef();


    for (int level = 0;
        level < 8;
        level++)
    {
        CONTROLTYPEID controlType = 0;


        current->get_CurrentControlType(
            &controlType
        );


        if (controlType ==
            UIA_DataItemControlTypeId ||
            controlType ==
            UIA_ListItemControlTypeId)
        {
            std::wstring name =
                GetElementName(
                    current
                );


            RECT bounds = {};


            HRESULT boundsResult =
                current->
                get_CurrentBoundingRectangle(
                    &bounds
                );


            if (!name.empty() &&
                SUCCEEDED(boundsResult))
            {
                info.name =
                    name;

                info.bounds =
                    bounds;


                std::wstring folder =
                    GetExplorerFolderPath(
                        explorerWindow
                    );


                info.path =
                    ResolveItemPath(
                        folder,
                        name
                    );


                info.found =
                    true;

                break;
            }
        }


        IUIAutomationElement* parent =
            nullptr;


        result =
            walker->GetParentElement(
                current,
                &parent
            );


        if (FAILED(result) || !parent)
        {
            break;
        }


        current->Release();

        current = parent;
    }


    current->Release();

    walker->Release();

    element->Release();

    automation->Release();


    return info;
}