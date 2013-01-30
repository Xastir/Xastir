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

#ifndef INTERFACEMANAGER_H
#define INTERFACEMANAGER_H

#include <QObject>
#include "packetinterface.h"
#include "netinterface.h"

class InterfaceManager : public QObject
{
    Q_OBJECT
public:
    explicit InterfaceManager(QObject *parent = 0);
    int numInterfaces() {return interfaces.count();}
    PacketInterface* getInterface(int i) {return interfaces[i];}
    NetInterface* getNewNetInterface() { return new NetInterface(interfaces.count()); }
    void addNewInterface( PacketInterface *iface );


signals:
    void interfaceAdded(PacketInterface *newInterface);

public slots:
    
private:
    QList<PacketInterface*> interfaces;
};

#endif // INTERFACEMANAGER_H
