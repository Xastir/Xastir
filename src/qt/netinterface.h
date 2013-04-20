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

#ifndef NETINTERFACE_H
#define NETINTERFACE_H

#include "packetinterface.h"
#include <QtNetwork>

class NetInterface : public PacketInterface
{
    Q_OBJECT

public:
    NetInterface(int iface = -1, QObject *parent = 0);
    void start(void);
    void stop(void);
    virtual QString deviceName();
    virtual QString deviceDescription();

    QString getHostName(void);
    void setHostName(QString);
    QString getPortString(void);
    void setPortString(QString);
    QString getCallSign(void);
    void setCallsign(QString);
    QString getPasscode(void);
    void setPasscode(QString newPasscode);
    QString getFilter(void);
    void setFilter(QString newFilter);


protected:
    virtual void saveSpecificSettings(QSettings& settings);
    virtual void restoreSpecificSettings(QSettings& settings);

    QTcpSocket tcpSocket;
    QString hostName;
    QString portString;
    QString callsign;  // Should this be in the base class??
    QString passcode;
    QString filter;

private slots:
    void connectToServer();
    void connectionClosedByServer();
    void error();
    void nowConnected();
    void closeConnection();
    void incomingData();

};

#endif // NETINTERFACE_H
