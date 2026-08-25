#include "ExplorerDetector.h"
#include <UIAutomation.h>

#pragma comment(lib, "uiautomationcore.lib")


// get the name from one UI element
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


// get the file row under the mouse
ExplorerItemInfo GetHoveredExplorerItem()
{
    ExplorerItemInfo info;

    POINT mouse;

    if (!GetCursorPos(&mouse))
    {
        return info;
    }


    IUIAutomation* automation = nullptr;

    HRESULT result = CoCreateInstance(
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

    result = automation->ElementFromPoint(
        mouse,
        &element
    );


    if (FAILED(result) || !element)
    {
        automation->Release();

        return info;
    }


    IUIAutomationTreeWalker* walker = nullptr;

    result = automation->get_ControlViewWalker(
        &walker
    );


    if (FAILED(result) || !walker)
    {
        element->Release();
        automation->Release();

        return info;
    }


    IUIAutomationElement* current = element;

    current->AddRef();


    // move upward until we reach the whole file row
    for (int level = 0; level < 8; level++)
    {
        CONTROLTYPEID controlType = 0;

        current->get_CurrentControlType(
            &controlType
        );


        if (controlType == UIA_DataItemControlTypeId ||
            controlType == UIA_ListItemControlTypeId)
        {
            std::wstring name =
                GetElementName(current);

            RECT bounds = {};

            HRESULT boundsResult =
                current->get_CurrentBoundingRectangle(
                    &bounds
                );


            if (!name.empty() &&
                SUCCEEDED(boundsResult))
            {
                info.name = name;
                info.bounds = bounds;
                info.found = true;

                break;
            }
        }


        IUIAutomationElement* parent = nullptr;

        result = walker->GetParentElement(
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