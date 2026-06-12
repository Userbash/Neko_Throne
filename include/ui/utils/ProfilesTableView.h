#pragma once

#include "include/ui/utils/MyTableWidget.h"
#include <QStringList>

class ProfilesTableView : public MyTableWidget
{
public:
    explicit ProfilesTableView(QWidget *parent = nullptr) : MyTableWidget(parent) {
        if (columnCount() < 5) {
            setColumnCount(5);
            setHorizontalHeaderLabels(QStringList{"Type", "Address", "Name", "Test", "Traffic"});
        }
    }
};
