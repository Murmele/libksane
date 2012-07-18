/* ============================================================
 *
 * This file is part of the KDE project
 *
 * Copyright (C) 2007-2012 by Kare Sars <kare.sars@iki.fi>
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

#ifndef KSaneCheckBox_h
#define KSaneCheckBox_h

#include "KSaneOptionWidget.h"

// Qt includes
#include <QCheckBox>
#include <QGridLayout>

/**
  *@author Kåre Särs
  */

/**
 * A wrapper for a checkBox
 */
class KSaneCheckBox : public KSaneOptionWidget
{
    Q_OBJECT

public:

   /**
    * Create the checkBox.
    *
    * \param parent parent widget
    * \param text is the text describing the checkBox.
    */
    KSaneCheckBox(QWidget *parent, const QString& text);
    ~KSaneCheckBox();
    void setChecked(bool);
    bool isChecked();

Q_SIGNALS:
    void toggled(bool);

private:

    QCheckBox *chbx;
};

#endif
