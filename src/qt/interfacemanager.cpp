/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
 * $Id$
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2013  The Xastir Group
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Look at the README for more information on the program.
 */

#include <QSettings>
#include <QtGlobal>
#include "interfacemanager.h"

InterfaceManager::InterfaceManager(QObject *parent) :
    QObject(parent)
{
}

void InterfaceManager::addNewInterface(PacketInterface *iface)
{
    interfaces.append(iface);
    interfaceAdded(iface);
}

void InterfaceManager::saveInterfaces()
{
    QSettings settings;
    QPointer<PacketInterface> ptr;
    int i = 0;

    settings.beginGroup("DeviceInterfaces");
    settings.remove(""); // Delete previous settings
    foreach(ptr, interfaces) {
        settings.beginGroup( "Interface" + QString::number(i++));
        ptr->saveSettings(settings);
        settings.endGroup();
    }
    settings.endGroup();
}

void InterfaceManager::restoreInterfaces()
{
    QSettings settings;
    QString deviceType;
    QString subgroup;
    QPointer<PacketInterface> ptr;
    int i = 0;

    settings.beginGroup("DeviceInterfaces");
    foreach(subgroup, settings.childGroups()) {
        settings.beginGroup(subgroup);
        deviceType = settings.value("interfaceClass").toString();
        qDebug(deviceType.toAscii());
        if(deviceType == "NetInterface")
        {
            ptr = new NetInterface(i++, this);
        }
        else
        {
            qWarning("Unknown Interface Type");
            settings.endGroup();
            break;
        }

        ptr->restoreFromSettings(settings);
        addNewInterface(ptr);
        settings.endGroup();
    }
}

