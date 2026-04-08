// SPDX-License-Identifier: GPL-2.0-or-later
#include "include/ui/core/TitleBar.hpp"

#include <QApplication>
#include <QGraphicsDropShadowEffect>
#include <QPainter>
#include <QScreen>
#include <QStyle>
#include <QStyleOption>
#include <QWindow>

// ---------------------------------------------------------------------------
// TitleBar widget
// ---------------------------------------------------------------------------

TitleBar::TitleBar(QWidget *parentWindow)
    : QWidget(parentWindow)
    , m_parentWindow(parentWindow)
{
    setObjectName(QStringLiteral("TitleBar"));
    setFixedHeight(38);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAttribute(Qt::WA_StyledBackground, true);
    setupUI();
}

void TitleBar::setupUI() {
    m_mainLayout = new QHBoxLayout(this);
    m_mainLayout->setContentsMargins(8, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // Icon
    m_iconLabel = new QLabel(this);
    m_iconLabel->setFixedSize(20, 20);
    m_iconLabel->setScaledContents(true);
    m_mainLayout->addWidget(m_iconLabel);
    m_mainLayout->addSpacing(8);

    // Left area for custom widgets
    m_leftLayout = new QHBoxLayout;
    m_leftLayout->setContentsMargins(0, 0, 0, 0);
    m_leftLayout->setSpacing(4);
    m_mainLayout->addLayout(m_leftLayout);

    // Title
    m_titleLabel = new QLabel(this);
    m_titleLabel->setObjectName(QStringLiteral("TitleBarLabel"));
    m_mainLayout->addWidget(m_titleLabel);

    // Flexible spacer before window buttons
    m_mainLayout->addStretch(1);

    // Window control buttons — minimal, CSS-styled
    auto makeBtn = [this](const QString &objectName, const QString &symbol) {
        auto *btn = new QPushButton(symbol, this);
        btn->setObjectName(objectName);
        btn->setFixedSize(46, 38);
        btn->setFlat(true);
        btn->setFocusPolicy(Qt::NoFocus);
        return btn;
    };

    m_btnMinimize = makeBtn(QStringLiteral("BtnMinimize"), QStringLiteral("\u2014")); // —
    m_btnMaximize = makeBtn(QStringLiteral("BtnMaximize"), QStringLiteral("\u25A1")); // □
    m_btnClose    = makeBtn(QStringLiteral("BtnClose"),    QStringLiteral("\u2715")); // ✕

    m_mainLayout->addWidget(m_btnMinimize);
    m_mainLayout->addWidget(m_btnMaximize);
    m_mainLayout->addWidget(m_btnClose);

    connect(m_btnMinimize, &QPushButton::clicked, this, [this] {
        m_parentWindow->showMinimized();
        emit minimizeClicked();
    });

    connect(m_btnMaximize, &QPushButton::clicked, this, [this] {
        if (m_parentWindow->isMaximized())
            m_parentWindow->showNormal();
        else
            m_parentWindow->showMaximized();
        updateMaximizeButton();
        emit maximizeClicked();
    });

    connect(m_btnClose, &QPushButton::clicked, this, [this] {
        m_parentWindow->close();
        emit closeClicked();
    });
}

void TitleBar::setTitle(const QString &title) {
    m_titleLabel->setText(title);
}

void TitleBar::setIcon(const QIcon &icon) {
    const qreal dpr = m_iconLabel->devicePixelRatioF();
    m_iconLabel->setPixmap(icon.pixmap(QSize(20, 20) * dpr));
}

void TitleBar::setTitleBarVisible(bool visible) {
    setVisible(visible);
}

void TitleBar::addLeftWidget(QWidget *w) {
    m_leftLayout->addWidget(w);
}

void TitleBar::setCenterWidget(QWidget *w) {
    if (m_centerWidget) {
        m_mainLayout->removeWidget(m_centerWidget);
        m_centerWidget->deleteLater();
    }
    m_centerWidget = w;
    // Insert after title, before the stretch
    m_mainLayout->insertWidget(m_mainLayout->indexOf(m_titleLabel) + 1, w, 1);
}

void TitleBar::applyPlatformWindowEffects(QWidget *window) {
#ifdef _WIN32
    if (Platform::isMicaSupported()) {
        Platform::enableMicaEffect(window);
    }
    // On Windows we use frameless hint; Snap Layouts handled via native event filter.
    window->setWindowFlags(window->windowFlags() | Qt::FramelessWindowHint);
#elif defined(__linux__)
    if (Platform::isWayland()) {
        Platform::configureLinuxCSD(window);
    } else {
        // X11: frameless
        window->setWindowFlags(window->windowFlags() | Qt::FramelessWindowHint);
    }
#endif
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_btnMaximize->click();
    }
}

void TitleBar::paintEvent(QPaintEvent * /*event*/) {
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void TitleBar::updateMaximizeButton() {
    if (m_parentWindow->isMaximized())
        m_btnMaximize->setText(QStringLiteral("\u29C9")); // ⧉ restore
    else
        m_btnMaximize->setText(QStringLiteral("\u25A1")); // □ maximize
}
