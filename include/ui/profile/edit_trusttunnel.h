#pragma once

#include <QWidget>
#include "profile_editor.h"

class EditTrustTunnel : public QWidget, public ProfileEditor {
public:
    explicit EditTrustTunnel(QWidget *parent = nullptr) : QWidget(parent) {}
    void onStart(std::shared_ptr<Configs::ProxyEntity>) override {}
    bool onEnd() override { return true; }
};
