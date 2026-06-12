#pragma once

#include <QWidget>
#include "profile_editor.h"

class EditJuicity : public QWidget, public ProfileEditor {
public:
    explicit EditJuicity(QWidget *parent = nullptr) : QWidget(parent) {}
    void onStart(std::shared_ptr<Configs::ProxyEntity>) override {}
    bool onEnd() override { return true; }
};
