#include <QStyle>
#include <QApplication>
#include <QPalette>

#include "include/ui/setting/ThemeManager.hpp"

ThemeManager *themeManager = new ThemeManager;

void ThemeManager::ApplyTheme(const QString &theme, bool force) {
    if (this->system_style_name.isEmpty()) {
        this->system_style_name = qApp->style()->name();
    }

    if (this->current_theme == theme && !force) {
        return;
    }

    auto lowerTheme = theme.toLower();
    if (lowerTheme == "system") {
        qApp->setStyleSheet("");
        qApp->setStyle(system_style_name);
    } else {
        qApp->setStyleSheet("");
        qApp->setStyle(theme);
    }

    current_theme = theme;

    emit themeChanged(theme);
}
