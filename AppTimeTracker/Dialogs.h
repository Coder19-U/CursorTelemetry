#pragma once
#include <string>

class Dialogs {
public:
    bool ShowConfirmClose(const std::wstring& title, const std::wstring& message) const;
    void ShowInfo(const std::wstring& title, const std::wstring& message) const;
};
