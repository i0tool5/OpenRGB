#include "OpenRGBDeviceInfoPage.h"

using namespace Ui;

OpenRGBDeviceInfoPage::OpenRGBDeviceInfoPage(RGBController *dev, QWidget *parent) :
    QFrame(parent),
    ui(new Ui::OpenRGBDeviceInfoPageUi)
{
    controller = dev;

    ui->setupUi(this);

    ui->TypeValue->setText(device_type_to_str(dev->type).c_str());

    ui->NameValue->setText(QString::fromStdString(dev->name));
    ui->DescriptionValue->setText(QString::fromStdString(dev->description));
    ui->VersionValue->setText(QString::fromStdString(dev->version));
    ui->LocationValue->setText(QString::fromStdString(dev->location));
    ui->SerialValue->setText(QString::fromStdString(dev->serial));
}

OpenRGBDeviceInfoPage::~OpenRGBDeviceInfoPage()
{
    delete ui;
}

RGBController* OpenRGBDeviceInfoPage::GetController()
{
    return controller;
}
