/**
 ******************************************************************************
 *
 * @file       versiondialog.cpp
 * @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
 *             Parts by Nokia Corporation (qt-info@nokia.com) Copyright (C) 2009.
 * @addtogroup GCSPlugins GCS Plugins
 * @{
 * @addtogroup CorePlugin Core Plugin
 * @{
 * @brief The Core GCS plugin
 *****************************************************************************/
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "versiondialog.h"
#include "version_info/version_info.h"
#include "coreconstants.h"
#include "icore.h"

#include <utils/qtcassert.h>

#include <QtCore/QDate>
#include <QtCore/QFile>
#include <QtCore/QSysInfo>

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextBrowser>

using namespace Core;
using namespace Core::Internal;
using namespace Core::Constants;

VersionDialog::VersionDialog(QWidget *parent)
    : QDialog(parent)
{
    // We need to set the window icon explicitly here since for some reason the
    // application icon isn't used when the size of the dialog is fixed (at least not on X11/GNOME)
    setWindowIcon(QIcon(":/core/images/openpilot_logo_32.png"));

    setWindowTitle(tr("About OpenPilot GCS"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    QGridLayout *layout = new QGridLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize);

    const QString description = tr(
        "<h3>OpenPilot Ground Control Station</h3>"
        "GCS Revision: <b>%1</b><br/>"
        "UAVO Hash: <b>%2</b><br/>"
        "<br/>"
        "Built from %3<br/>"
        "Built on %4 at %5<br/>"
        "Based on Qt %6 (%7 bit)<br/>"
        "<br/>"
        "&copy; %8, 2010-%9. All rights reserved.<br/>"
        "<br/>"
        "<small>This program is free software; you can redistribute it and/or modify<br/>"
        "it under the terms of the GNU General Public License as published by<br/>"
        "the Free Software Foundation; either version 3 of the License, or<br/>"
        "(at your option) any later version.<br/>"
        "<br/>"
        "The program is provided AS IS with NO WARRANTY OF ANY KIND, "
        "INCLUDING THE WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A "
        "PARTICULAR PURPOSE.</small>"
        ).arg(
        VersionInfo::revision().left(60), // %1
        VersionInfo::uavoHash().left(8), // %2
        VersionInfo::origin(), // $3
        QLatin1String(__DATE__), // %4
        QLatin1String(__TIME__), // %5
        QLatin1String(QT_VERSION_STR), // %6
        QString::number(QSysInfo::WordSize), // %7
        QLatin1String(GCS_AUTHOR), // %8
        VersionInfo::year() // %9
        );

    QLabel *copyRightLabel = new QLabel(description);
    copyRightLabel->setWordWrap(true);
    copyRightLabel->setOpenExternalLinks(true);
    copyRightLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    QPushButton *closeButton    = buttonBox->button(QDialogButtonBox::Close);
    QTC_ASSERT(closeButton, /**/);
    buttonBox->addButton(closeButton, QDialogButtonBox::ButtonRole(QDialogButtonBox::RejectRole | QDialogButtonBox::AcceptRole));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    QLabel *logoLabel = new QLabel;
    logoLabel->setPixmap(QPixmap(QLatin1String(":/core/images/openpilot_logo_128.png")));
    layout->addWidget(logoLabel, 0, 0, 1, 1);
    layout->addWidget(copyRightLabel, 0, 1, 4, 4);
    layout->addWidget(buttonBox, 4, 0, 1, 5);
}
