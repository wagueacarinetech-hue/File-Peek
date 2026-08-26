#pragma once

#include <windows.h>
#include <string>

struct ExplorerItemInfo
{
    std::wstring name;
    std::wstring path;
    RECT bounds = {};
    bool found = false;
};

ExplorerItemInfo GetHoveredExplorerItem();