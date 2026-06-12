#pragma once

#include <QWidget>
#include "profile_editor.h"

class EditDirect : public QWidget, public ProfileEditor {
public:
    explicit EditDirect(QWidget *parent = nullptr) : QWidget(parent) {}
    void onStart(std::shared_ptr<Configs::ProxyEntity>) override {}
    bool onEnd() override { return true; }
};
