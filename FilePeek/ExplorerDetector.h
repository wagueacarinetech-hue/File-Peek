#pragma once

#include <windows.h>
#include <string>

struct ExplorerItemInfo
{
    std::wstring name;
    RECT bounds;
    bool found = false;
};

ExplorerItemInfo GetHoveredExplorerItem();