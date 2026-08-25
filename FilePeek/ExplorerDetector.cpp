#include <windows.h>
#include <UIAutomation.h>
#include <string>

#pragma comment(lib, "uiautomationcore.lib")


// try to get a useful name from one element
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


// move up until we find the explorer row/item
std::wstring GetHoveredExplorerItemName()
{
    POINT mouse;

    if (!GetCursorPos(&mouse))
    {
        return L"";
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
        return L"";
    }


    IUIAutomationElement* element = nullptr;

    result = automation->ElementFromPoint(
        mouse,
        &element
    );


    if (FAILED(result) || !element)
    {
        automation->Release();

        return L"";
    }


    IUIAutomationTreeWalker* walker = nullptr;

    result = automation->get_ControlViewWalker(
        &walker
    );


    if (FAILED(result) || !walker)
    {
        element->Release();
        automation->Release();

        return L"";
    }


    IUIAutomationElement* current = element;

    current->AddRef();


    std::wstring itemName;


    // walk up through parents
    for (int level = 0; level < 8; level++)
    {
        CONTROLTYPEID controlType = 0;

        current->get_CurrentControlType(
            &controlType
        );


        // explorer items are often DataItem or ListItem
        if (controlType == UIA_DataItemControlTypeId ||
            controlType == UIA_ListItemControlTypeId)
        {
            itemName =
                GetElementName(current);

            if (!itemName.empty())
            {
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


    return itemName;
}