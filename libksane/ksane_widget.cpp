/* ============================================================
 *
 * This file is part of the KDE project
 *
 * Date        : 2009-01-24
 * Description : Sane interface for KDE
 *
 * Copyright (C) 2007-2009 by Kare Sars <kare dot sars at iki dot fi>
 * Copyright (C) 2009 by Grzegorz Kurtyka <grzegorz dot kurtyka at gmail dot com>
 * Copyright (C) 2007-2008 by Gilles Caulier <caulier dot gilles at gmail dot com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) version 3, or any
 * later version accepted by the membership of KDE e.V. (or its
 * successor approved by the membership of KDE e.V.), which shall
 * act as a proxy defined in Section 6 of version 3 of the license.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * ============================================================ */

#include "ksane.h"
#include "ksane.moc"

// Qt includes
#include <QApplication>
#include <QVarLengthArray>
#include <QLabel>

// KDE includes
#include <kpassworddialog.h>
#include <kwallet.h>
#include <kpushbutton.h>

// Local includes.
#include "ksane_widget_private.h"
#include "ksane_option.h"
#include "ksane_opt_button.h"
#include "ksane_opt_checkbox.h"
#include "ksane_opt_combo.h"
#include "ksane_opt_entry.h"
#include "ksane_opt_fslider.h"
#include "ksane_opt_gamma.h"
#include "ksane_opt_slider.h"
#include "labeled_separator.h"
#include "radio_select.h"
#include "labeled_gamma.h"

#define SCALED_PREVIEW_MAX_SIDE 400
#define MAX_NUM_OPTIONS 100
#define IMG_DATA_R_SIZE 100000

namespace KSaneIface
{

/** static function called by sane_open to get authorization from user */
static void getSaneAuthorization(SANE_String_Const resource, SANE_Char *username, SANE_Char *password) {
    KWallet::Wallet *sane_wallet;
    QString my_folder_name("ksane");
    QMap<QString, QString> wallet_entry;
    QString wallet_entry_key, resource_rest;
    wallet_entry_key = QString(resource).section(":", 0, 1);
    resource_rest = QString(resource).section(":", 2);

    QStringList resource_rest_list = resource_rest.split("$");
    wallet_entry_key.append(":").append(resource_rest_list.at(0) );

    qDebug() << "sane resource " << QString(resource) << " resource_rest " << resource_rest;

    KPasswordDialog *dlg;

    ///FIXME Skanlite(7539) KWallet::Wallet::openWallet: Pass a valid window to KWallet::Wallet::openWallet().
    sane_wallet = KWallet::Wallet::openWallet(KWallet::Wallet::LocalWallet(), 0);

    if(sane_wallet) {
        dlg = new KPasswordDialog(0, KPasswordDialog::ShowUsernameLine | KPasswordDialog::ShowKeepPassword);
        if(!sane_wallet->hasFolder(my_folder_name)) {
            sane_wallet->createFolder(my_folder_name);
        }

        sane_wallet->setFolder(my_folder_name);
        sane_wallet->readMap(wallet_entry_key, wallet_entry);
        if(!wallet_entry.empty() || true) {
            dlg->setUsername( wallet_entry["username"] );
            dlg->setPassword( wallet_entry["password"] );
            dlg->setKeepPassword( true );
        }
    } else {
        dlg = new KPasswordDialog(0, KPasswordDialog::ShowUsernameLine);
    }

    dlg->setPrompt(i18n("Authentication required for: %1", wallet_entry_key ) );

    if( !dlg->exec() )
        return; //the user canceled

    if(dlg->keepPassword()) {
      QMap<QString, QString> entry;
      entry["username"] = dlg->username().toUtf8();
      entry["password"] = dlg->password().toUtf8();
      sane_wallet->writeMap(wallet_entry_key, entry);

    }

    qstrncpy(username, dlg->username().toUtf8(), SANE_MAX_USERNAME_LEN );
    qstrncpy(password, dlg->password().toUtf8(), SANE_MAX_PASSWORD_LEN );
    delete dlg;
}

    
KSaneWidget::KSaneWidget(QWidget* parent)
    : QWidget(parent), d(new KSaneWidgetPrivate)
{
    SANE_Int    version;
    SANE_Status status;

    //kDebug(51004) <<  "The language is:" << KGlobal::locale()->language();
    //kDebug(51004) <<  "Languagelist" << KGlobal::locale()->languageList();
    KGlobal::locale()->insertCatalog("libksane");
    KGlobal::locale()->insertCatalog("sane-backends");

    status = sane_init(&version, &getSaneAuthorization);
    if (status != SANE_STATUS_GOOD) {
        kDebug(51004) << "libksane: sane_init() failed("
                 << sane_strstatus(status) << ")";
    }
    else {
        //kDebug(51004) << "Sane Version = "
        //         << SANE_VERSION_MAJOR(version) << "."
        //         << SANE_VERSION_MINOR(version) << "."
        //         << SANE_VERSION_BUILD(version);
    }

    d->m_readValsTmr.setSingleShot(true);
    d->m_startScanTmr.setSingleShot(true);
    d->m_readDataTmr.setSingleShot(true);
    connect(&d->m_readValsTmr,   SIGNAL(timeout()), d, SLOT(valReload()));
    connect(&d->m_startScanTmr,  SIGNAL(timeout()), d, SLOT(startScan()));
    connect(&d->m_readDataTmr,   SIGNAL(timeout()), d, SLOT(processData()));

    // Forward signals from the private class
    connect(d, SIGNAL(scanProgress(int)), this, SIGNAL(scanProgress(int)));
    connect(d, SIGNAL(imageReady(QByteArray &, int, int, int, int)),
             this, SIGNAL(imageReady(QByteArray &, int, int, int, int)));

    // Create the static UI
    QHBoxLayout *base_layout = new QHBoxLayout;
    base_layout->setSpacing(2);
    base_layout->setMargin(0);
    setLayout(base_layout);
    QVBoxLayout *options_layout = new QVBoxLayout;
    options_layout->setSpacing(2);
    options_layout->setMargin(0);
    QVBoxLayout *preview_layout = new QVBoxLayout;
    preview_layout->setSpacing(2);
    preview_layout->setMargin(0);
    
    base_layout->addLayout(options_layout, 0);
    base_layout->addLayout(preview_layout, 100);

    // create the preview
    d->m_previewViewer = new KSaneViewer(this);
    connect(d->m_previewViewer, SIGNAL(newSelection(float, float, float, float)),
            d, SLOT(handleSelection(float, float, float, float)));

    d->m_zInBtn  = new KPushButton(this);
    d->m_zInBtn->setIcon(KIcon("zoom-in"));
    d->m_zInBtn->setToolTip(i18n("Zoom In"));
    d->m_zOutBtn = new KPushButton(this);
    d->m_zOutBtn->setIcon(KIcon("zoom-out"));
    d->m_zOutBtn->setToolTip(i18n("Zoom Out"));
    d->m_zSelBtn = new KPushButton(this);
    d->m_zSelBtn->setIcon(KIcon("zoom-fit-best"));
    d->m_zSelBtn->setToolTip(i18n("Zoom to Selection"));
    d->m_zFitBtn = new KPushButton(this);
    d->m_zFitBtn->setIcon(KIcon("document-preview"));
    d->m_zFitBtn->setToolTip(i18n("Zoom to Fit"));

    d->m_warmingUp = new QLabel(this);
    d->m_warmingUp->setText(i18n("The lamp is warming up!"));
    d->m_warmingUp->setAlignment(Qt::AlignCenter);
    d->m_warmingUp->setAutoFillBackground(true);
    d->m_warmingUp->setBackgroundRole(QPalette::Highlight);
    //d->m_warmingUp->setForegroundRole(QPalette::HighlightedText);
    d->m_warmingUp->hide();

    d->m_progressBar = new QProgressBar(this);
    d->m_progressBar->hide();
    d->m_progressBar->setMaximum(100);

    d->m_cancelBtn   = new KPushButton(this);
    d->m_cancelBtn->setIcon(KIcon("process-stop"));
    d->m_cancelBtn->setToolTip(i18n("Cancel current scan operation"));
    d->m_cancelBtn->hide();

    d->m_prevBtn = new KPushButton(this);
    d->m_prevBtn->setIcon(KIcon("document-import"));
    d->m_prevBtn->setToolTip(i18n("Scan Preview Image"));
    d->m_prevBtn->setText(i18nc("Preview button text", "Preview"));
    d->m_scanBtn = new KPushButton(this);
    d->m_scanBtn->setIcon(KIcon("document-save"));
    d->m_scanBtn->setToolTip(i18n("Scan Final Image"));
    d->m_scanBtn->setText(i18nc("Final scan button text", "Scan"));
    d->m_scanBtn->setFocus(Qt::OtherFocusReason);

    connect(d->m_zInBtn,    SIGNAL(clicked()), d->m_previewViewer, SLOT(zoomIn()));
    connect(d->m_zOutBtn,   SIGNAL(clicked()), d->m_previewViewer, SLOT(zoomOut()));
    connect(d->m_zSelBtn,   SIGNAL(clicked()), d->m_previewViewer, SLOT(zoomSel()));
    connect(d->m_zFitBtn,   SIGNAL(clicked()), d->m_previewViewer, SLOT(zoom2Fit()));
    connect(d->m_scanBtn,   SIGNAL(clicked()), d, SLOT(scanFinal()));
    connect(d->m_prevBtn,   SIGNAL(clicked()), d, SLOT(scanPreview()));
    connect(d->m_cancelBtn, SIGNAL(clicked()), this, SLOT(scanCancel()));

    QHBoxLayout *zoom_layout = new QHBoxLayout;
    QHBoxLayout *progress_lay = new QHBoxLayout;

    preview_layout->addWidget(d->m_previewViewer, 100);
    preview_layout->addLayout(progress_lay, 0);
    preview_layout->addLayout(zoom_layout, 0);

    progress_lay->addWidget(d->m_warmingUp, 100);
    progress_lay->addWidget(d->m_progressBar, 100);
    progress_lay->addWidget(d->m_cancelBtn, 0);

    zoom_layout->addWidget(d->m_zInBtn);
    zoom_layout->addWidget(d->m_zOutBtn);
    zoom_layout->addWidget(d->m_zSelBtn);
    zoom_layout->addWidget(d->m_zFitBtn);
    zoom_layout->addStretch(100);
    zoom_layout->addWidget(d->m_prevBtn);
    zoom_layout->addWidget(d->m_scanBtn);

    // Create Options Widget
    d->m_optsTabWidget = new KTabWidget;
    options_layout->addWidget(d->m_optsTabWidget, 0);
    
    // Add the basic options tab
    d->m_basicScrollA = new QScrollArea;
    d->m_basicScrollA->setWidgetResizable(true);
    d->m_basicScrollA->setFrameShape(QFrame::NoFrame);
    d->m_optsTabWidget->addTab(d->m_basicScrollA, i18n("Basic Options"));

    // Add the other options tab
    d->m_otherScrollA = new QScrollArea;
    d->m_otherScrollA->setWidgetResizable(true);
    d->m_otherScrollA->setFrameShape(QFrame::NoFrame);
    d->m_optsTabWidget->addTab(d->m_otherScrollA, i18n("Other Options"));

}

KSaneWidget::~KSaneWidget()
{
    closeDevice();
    delete d;
}

QString KSaneWidget::vendor() const {return d->m_vendor;}
QString KSaneWidget::make() const {return d->m_vendor;}
QString KSaneWidget::model() const {return d->m_model;}

QString KSaneWidget::selectDevice(QWidget* parent)
{
    int                 i=0;
    QStringList         dev_name_list;
    QString             tmp;
    SANE_Status         status;
    SANE_Device const **dev_list;

    status = sane_get_devices(&dev_list, SANE_TRUE);

    while(dev_list[i] != 0) {
        //kDebug(51004) << "i="       << i << " "
        //         << "name='"   << dev_list[i]->name   << "' "
        //         << "vendor='" << dev_list[i]->vendor << "' "
        //         << "model='"  << dev_list[i]->model  << "' "
        //         << "type='"   << dev_list[i]->type   << "'";
        tmp = QString(dev_list[i]->name);
        tmp += '\n' + QString(dev_list[i]->vendor);
        tmp += " : " + QString(dev_list[i]->model);
        dev_name_list += tmp;
        i++;
    }

    if (dev_name_list.isEmpty()) {
        KMessageBox::sorry(0, i18n("No scanner device has been found."));
        return QString();
    }

    if (dev_name_list.count() == 1) {
        // don't bother asking the user: we only have one choice!
        return dev_list[0]->name;
    }

    RadioSelect sel;
    sel.setWindowTitle(qApp->applicationName());
    i = sel.getSelectedIndex(parent, i18n("Select Scanner"), dev_name_list, 0);
    //kDebug(51004) << "i=" << i;

    if ((i < 0) || (i >= dev_name_list.count())) {
        return QString();
    }

    return QString(dev_list[i]->name);
}

bool KSaneWidget::openDevice(const QString &device_name)
{
    int                            i=0;
    const SANE_Option_Descriptor  *optDesc;
    SANE_Status                    status;
    SANE_Word                      num_sane_options;
    SANE_Int                       res;
    SANE_Device const            **dev_list;

    // don't bother trying to open if the device string is empty
    if (device_name.isEmpty()) {
        return false;
    }
    // get the device list to get the vendor and model info
    status = sane_get_devices(&dev_list, SANE_TRUE);

    while(dev_list[i] != 0) {
        if (QString(dev_list[i]->name) == device_name) {
            d->m_modelName = QString(dev_list[i]->vendor) + ' ' + QString(dev_list[i]->model);
            d->m_vendor    = QString(dev_list[i]->vendor);
            d->m_model     = QString(dev_list[i]->model);
            break;
        }
        i++;
    }

    if (device_name == "test") {
        d->m_modelName = "Test Scanner";
        d->m_vendor    = "Test";
        d->m_model     = "Scanner";
    }

    // Try to open the device
    if (sane_open(device_name.toLatin1(), &d->m_saneHandle) != SANE_STATUS_GOOD) {
        //kDebug(51004) << "sane_open(\"" << device_name << "\", &handle) failed!";
        return false;
    }

    // Read the options (start with option 0 the number of parameters)
    optDesc = sane_get_option_descriptor(d->m_saneHandle, 0);
    if (optDesc == 0) {
        return false;
    }
    QVarLengthArray<char> data(optDesc->size);
    status = sane_control_option(d->m_saneHandle, 0, SANE_ACTION_GET_VALUE, data.data(), &res);
    if (status != SANE_STATUS_GOOD) {
        return false;
    }
    num_sane_options = *reinterpret_cast<SANE_Word*>(data.data());

    // read the rest of the options
    for (i=1; i<num_sane_options; i++) {
        switch (KSaneOption::otpionType(sane_get_option_descriptor(d->m_saneHandle, i))) {
            case KSaneOption::TYPE_DETECT_FAIL:
                d->m_optList.append(new KSaneOption(d->m_saneHandle, i));
                break;
            case KSaneOption::TYPE_CHECKBOX:
                d->m_optList.append(new KSaneOptCheckBox(d->m_saneHandle, i));
                break;
            case KSaneOption::TYPE_SLIDER:
                d->m_optList.append(new KSaneOptSlider(d->m_saneHandle, i));
                break;
            case KSaneOption::TYPE_F_SLIDER:
                d->m_optList.append(new KSaneOptFSlider(d->m_saneHandle, i));
                break;
            case KSaneOption::TYPE_COMBO:
                d->m_optList.append(new KSaneOptCombo(d->m_saneHandle, i));
                break;
            case KSaneOption::TYPE_ENTRY:
                d->m_optList.append(new KSaneOptEntry(d->m_saneHandle, i));
                break;
            case KSaneOption::TYPE_GAMMA:
                d->m_optList.append(new KSaneOptGamma(d->m_saneHandle, i));
                break;
            case KSaneOption::TYPE_BUTTON:
                d->m_optList.append(new KSaneOptButton(d->m_saneHandle, i));
                break;
        }
    }

    // do the connections of the option parameters
    for (i=1; i<d->m_optList.size(); i++) {
        connect (d->m_optList.at(i), SIGNAL(optsNeedReload()), d, SLOT(optReload()));
        connect (d->m_optList.at(i), SIGNAL(valsNeedReload()), d, SLOT(scheduleValReload()));
    }

    // Create the options interface
    d->createOptInterface();

    // try to set KSaneWidget default values
    d->setDefaultValues();

    // estimate the preview size and create an empty image
    // this is done so that you can select scanarea without
    // having to scan a preview.
    d->updatePreviewSize();

    return true;
}


bool KSaneWidget::closeDevice()
{
    d->scanCancel();
    d->clearDeviceOptions();
    sane_close(d->m_saneHandle);
    return true;
}

QImage KSaneWidget::toQImage(const QByteArray &data,
                              int width,
                              int height,
                              int bytes_per_line,
                              ImageFormat format)
{
    QImage img;
    int j=0;
    int pixel_x = 0;
    int pixel_y = 0;

    switch (format)
    {
        case FormatBlackWhite:
            img = QImage((uchar*)data.data(),
                          width,
                          height,
                          bytes_per_line,
                          QImage::Format_Mono);
            for (int i=0; i<img.height()*img.bytesPerLine(); i++) {
                img.bits()[i] = ~img.bits()[i];
            }
            return img;

        case FormatGrayScale8:
            img = QImage(width,
                         height,
                         QImage::Format_RGB32);
            j=0;
            for (int i=0; i<data.size(); i++) {
                img.bits()[j+0] = data.data()[i];
                img.bits()[j+1] = data.data()[i];
                img.bits()[j+2] = data.data()[i];
                j+=4;
            }
            return img;

        case FormatGrayScale16:
            img = QImage(width,
                         height,
                         QImage::Format_RGB32);
            j=0;
            for (int i=1; i<data.size(); i+=2) {
                img.bits()[j+0] = data.data()[i];
                img.bits()[j+1] = data.data()[i];
                img.bits()[j+2] = data.data()[i];
                j+=4;
            }
            KMessageBox::sorry(0, i18n("The image data contained 16 bits per color, "
                    "but the color depth has been truncated to 8 bits per color."));
            return img;

        case FormatRGB_8_C:
            pixel_x = 0;
            pixel_y = 0;

            img = QImage(width,
                         height,
                         QImage::Format_RGB32);

            for (int i=0; i<data.size(); i+=3) {
                img.setPixel(pixel_x,
                             pixel_y,
                             qRgb(data[i],
                                  data[i+1],
                                  data[i+2]));

                inc_pixel(pixel_x, pixel_y, width);
            }
            return img;

        case FormatRGB_16_C:
            pixel_x = 0;
            pixel_y = 0;

            img = QImage(width,
                         height,
                         QImage::Format_RGB32);

            for (int i=1; i<data.size(); i+=6) {
                img.setPixel(pixel_x,
                             pixel_y,
                             qRgb(data[i],
                                  data[i+2],
                                  data[i+4]));

                inc_pixel(pixel_x, pixel_y, width);
            }
            KMessageBox::sorry(0, i18n("The image data contained 16 bits per color, "
                    "but the color depth has been truncated to 8 bits per color."));
            return img;

        case FormatNone:
            break;
    }
    kDebug(51004) << "Unsupported conversion";
    return img;
}

void KSaneWidget::scanFinal()
{
    d->scanFinal();
}

void KSaneWidget::scanCancel()
{
    d->scanCancel();
    emit scanProgress(0);
}

void KSaneWidget::getOptVals(QMap <QString, QString> &opts)
{
    KSaneOption *option;
    opts.clear();
    QString tmp;

    for (int i=1; i<d->m_optList.size(); i++) {
        option = d->m_optList.at(i);
        if (option->getValue(tmp)) {
            opts[option->name()] = tmp;
        }
    }
}

bool KSaneWidget::getOptVal(const QString &optname, QString &value)
{
    KSaneOption *option;

    if ((option = d->getOption(optname)) != 0) {
        return option->getValue(value);
    }
    return false;
}

int KSaneWidget::setOptVals(const QMap <QString, QString> &opts)
{
    QString tmp;
    int i;
    int ret=0;

    for (i=0; i<d->m_optList.size(); i++) {
        if (opts.contains(d->m_optList.at(i)->name())) {
            tmp = opts[d->m_optList.at(i)->name()];
            if (d->m_optList.at(i)->setValue(tmp) == false) {
                ret++;
            }
        }
    }
    return ret;
}

bool KSaneWidget::setOptVal(const QString &option, const QString &value)
{
    KSaneOption *opt;

    if ((opt = d->getOption(option)) != 0) {
        if (opt->setValue(value)) {
            return true;
        }
    }

    return false;
}

void KSaneWidget::setScanButtonText(const QString &scanLabel)
{
    if (d->m_scanBtn == 0) {
        kError() << "setScanButtonText was called before KSaneWidget was initialized";
        return;
    }
    d->m_scanBtn->setText(scanLabel);
}

void KSaneWidget::setPreviewButtonText(const QString &previewLabel)
{
    if (d->m_scanBtn == 0) {
        kError() << "setPreviewButtonText was called before KSaneWidget was initialized";
        return;
    }
    d->m_prevBtn->setText(previewLabel);
}

void KSaneWidget::enableAutoSelect(bool enable)
{
    d->m_autoSelect = enable;
}

}  // NameSpace KSaneIface