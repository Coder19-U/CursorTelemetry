#include "Dialogs.h"

#include <Windows.h>
#include <CommCtrl.h>

bool Dialogs::ShowConfirmClose(const std::wstring& title, const std::wstring& message) const {
    TASKDIALOG_BUTTON buttons[] = {
        {1001, L"Закрыть сейчас"},
        {1002, L"Отмена"}
    };

    TASKDIALOGCONFIG cfg{};
    cfg.cbSize = sizeof(cfg);
    cfg.hwndParent = nullptr;
    cfg.dwFlags = TDF_POSITION_RELATIVE_TO_WINDOW;
    cfg.pszWindowTitle = title.c_str();
    cfg.pszMainInstruction = L"Пожалуйста, сохраните данные";
    cfg.pszContent = message.c_str();
    cfg.pButtons = buttons;
    cfg.cButtons = ARRAYSIZE(buttons);
    cfg.nDefaultButton = 1002;

    int pressed = 0;
    const HRESULT hr = TaskDialogIndirect(&cfg, &pressed, nullptr, nullptr);
    return SUCCEEDED(hr) && pressed == 1001;
}

void Dialogs::ShowInfo(const std::wstring& title, const std::wstring& message) const {
    TASKDIALOGCONFIG cfg{};
    cfg.cbSize = sizeof(cfg);
    cfg.dwFlags = TDF_POSITION_RELATIVE_TO_WINDOW;
    cfg.pszWindowTitle = title.c_str();
    cfg.pszMainInstruction = title.c_str();
    cfg.pszContent = message.c_str();
    TaskDialogIndirect(&cfg, nullptr, nullptr, nullptr);
}
