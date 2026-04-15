#include "include/ui/setting/dialog_vpn_settings.h"

#include "include/configs/proxy/Preset.hpp"
#include "include/global/GuiUtils.hpp"
#include "include/global/Configs.hpp"
#include "include/sys/NetworkLeakGuard.hpp"
#include "include/ui/mainwindow_interface.h"
#ifdef Q_OS_WIN
#include "include/sys/windows/WinVersion.h"
#endif

#include <QMessageBox>
#define ADJUST_SIZE runOnThread([=,this] { adjustSize(); adjustPosition(mainwindow); }, this);
DialogVPNSettings::DialogVPNSettings(QWidget *parent) : QDialog(parent), ui(new Ui::DialogVPNSettings) {
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    ADD_ASTERISK(this);

#ifdef Q_OS_WIN
    if (WinVersion::IsBuildNumGreaterOrEqual(BuildNumber::Windows_10_1507)) {
        ui->vpn_implementation->addItems(Preset::SingBox::VpnImplementation);
        ui->vpn_implementation->setCurrentText(Configs::dataStore->vpn_implementation);
    }
    else {
        ui->vpn_implementation->addItems(Preset::SingBox::VpnImplementation);
        ui->vpn_implementation->setCurrentText("gvisor");
        ui->vpn_implementation->setEnabled(false);
    }
#else
    ui->vpn_implementation->addItems(Preset::SingBox::VpnImplementation);
    ui->vpn_implementation->setCurrentText(Configs::dataStore->vpn_implementation);
#endif
    ui->vpn_mtu->setCurrentText(Int2String(Configs::dataStore->vpn_mtu));
    ui->vpn_ipv6->setChecked(Configs::dataStore->vpn_ipv6);
    ui->strict_route->setChecked(Configs::dataStore->vpn_strict_route);
    ui->tun_routing->setChecked(Configs::dataStore->enable_tun_routing);
    ADJUST_SIZE
}

DialogVPNSettings::~DialogVPNSettings() {
    delete ui;
}

void DialogVPNSettings::accept() {
    //
    auto mtu = ui->vpn_mtu->currentText().toInt();
    if (mtu > 10000 || mtu < 1000) mtu = 9000;
    Configs::dataStore->vpn_implementation = ui->vpn_implementation->currentText();
    Configs::dataStore->vpn_mtu = mtu;
    Configs::dataStore->vpn_ipv6 = ui->vpn_ipv6->isChecked();
    Configs::dataStore->vpn_strict_route = ui->strict_route->isChecked();
    Configs::dataStore->enable_tun_routing = ui->tun_routing->isChecked();
    //
    QStringList msg{"UpdateDataStore"};
    msg << "VPNChanged";
    MW_dialog_message("", msg.join(","));
    QDialog::accept();
}

void DialogVPNSettings::on_troubleshooting_clicked() {
    QMessageBox msg(
        QMessageBox::Information,
        tr("Troubleshooting"),
        tr("If you have trouble starting VPN, you can force reset Core process here.\n\n"
           "Reset Network: Manually restore IPv6 and clean up stale routes (useful if Core crashed).\n"
           "Reset Privilege Cache: Re-attempt using setcap if it was previously marked as broken."),
        QMessageBox::NoButton,
        this
    );
    auto resetCore = msg.addButton(tr("Reset Core"), QMessageBox::ActionRole);
    auto resetNet = msg.addButton(tr("Reset Network"), QMessageBox::ActionRole);
    auto resetPriv = msg.addButton(tr("Reset Privilege Cache"), QMessageBox::ActionRole);
    auto cancel = msg.addButton(tr("Cancel"), QMessageBox::RejectRole);

    msg.setDefaultButton(cancel);
    msg.setEscapeButton(cancel);

    msg.exec();
    auto clicked = msg.clickedButton();

    if (clicked == resetCore) {
        auto mw = GetMainWindow();
        if (mw != nullptr) {
            mw->StopVPNProcess();
        }
    } else if (clicked == resetNet) {
        NetworkLeakGuard::instance()->restoreIPv6();
        QMessageBox::information(this, tr("Success"), tr("Emergency network reset initiated. IPv6 restoration requested."));
    } else if (clicked == resetPriv) {
        Configs::dataStore->tun_setcap_broken = false;
        Configs::dataStore->pkexec_cancel_count = 0;
        Configs::dataStore->Save();
        QMessageBox::information(this, tr("Success"), tr("Privilege cache has been reset."));
    }
}
