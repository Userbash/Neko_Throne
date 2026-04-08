// SPDX-License-Identifier: GPL-2.0-or-later
// TranslationManager — Hot-swappable external .qm translation loader
// Scans lang/ directory, supports runtime language switching without restart.

#pragma once

#include <QObject>
#include <QTranslator>
#include <QFileSystemWatcher>
#include <QMap>
#include <QString>
#include <QStringList>

struct LanguageInfo {
    QString locale;      // e.g. "ru_RU"
    QString displayName; // e.g. "Русский"
    QString filePath;    // absolute path to .qm file
};

class TranslationManager : public QObject {
    Q_OBJECT

public:
    static TranslationManager *instance();

    // Scan lang/ directory and populate available languages.
    // Call once at startup after QApplication is created.
    void initialize();

    // Return the directory where .qm files are loaded from.
    QString translationsPath() const;

    // List of all discovered languages (locale code → info).
    QMap<QString, LanguageInfo> availableLanguages() const;

    // Currently active locale (empty string = system default / English fallback).
    QString currentLocale() const;

    // Switch UI language at runtime. Installs new translator, removes old one,
    // and sends QEvent::LanguageChange to all top-level widgets.
    // Returns true on success.
    bool switchLanguage(const QString &locale);

    // Re-scan lang/ folder for new .qm files added at runtime.
    void rescanLanguages();

signals:
    // Emitted after a successful language switch. Connect widgets/QML to retranslate.
    void languageChanged(const QString &newLocale);

    // Emitted when the set of available languages changes (files added/removed).
    void availableLanguagesChanged();

private:
    explicit TranslationManager(QObject *parent = nullptr);
    ~TranslationManager() override;

    void scanDirectory();
    void installTranslator(const QString &qmPath);
    void removeCurrentTranslator();

    // Derive a human-readable name from QLocale for the given locale string.
    static QString displayNameForLocale(const QString &locale);

    QString m_langDir;
    QString m_currentLocale;
    QTranslator *m_appTranslator = nullptr;
    QTranslator *m_qtTranslator = nullptr;
    QFileSystemWatcher *m_watcher = nullptr;
    QMap<QString, LanguageInfo> m_languages;

    Q_DISABLE_COPY_MOVE(TranslationManager)
};
