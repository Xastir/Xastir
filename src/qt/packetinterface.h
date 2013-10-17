/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
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


#ifndef PACKETINTERFACE_H
#define PACKETINTERFACE_H

#include <QObject>
#include <QSettings>

class PacketInterface : public QObject
{
    Q_OBJECT
public:
    explicit PacketInterface(int ifaceNumber = 0, QObject *parent = 0);

public:
    enum Device_Active {
        DEVICE_NOT_IN_USE,
        DEVICE_IN_USE
    };

    enum Device_Status {
        DEVICE_DOWN,
        DEVICE_STARTING,
        DEVICE_UP,
        DEVICE_ERROR
    };

    virtual void start() = 0;
    virtual void stop() = 0;

    /*! \brief Save interface settings using QSettings
     *
     *     This will save this interfaces settings in a platform independant
     *  manner using QSettings. It is assumed that the caller has created a
     * settings group before calling as appropriate. (For example, each interface
     * should be in a seperate group to avoid collision.)
     *
     * This function will call saveSpecificSettings, so any derived classes can
     * put their class specific settings in an overriden version of saveSpecificSettings.)
     */
    void saveSettings(QSettings& settings);

    /*! \brief Restore interface settings using QSettings
     *
     *     This will retreive interface settings in a platform independant
     *  manner using QSettings. It is assumed that the caller has set a
     * settings group before calling as appropriate. (For example, each interface
     * should be in a seperate group to avoid collision.)
     *
     * This function will call restoreSpecificSettings, so any derived classes can
     * handle their class specific settings in an overriden version of saveSpecificSettings.)
     */
    void restoreFromSettings(QSettings& settings);

    bool transmitAllowed();
    Device_Status deviceStatus();

    virtual QString deviceName() = 0;
    virtual QString deviceDescription() = 0;

    bool getActivateOnStartup() {return activateOnStartup;}
    void setActivateOnStartup(bool newValue) { activateOnStartup = newValue; }
    int getInterfaceNumber() { return interfaceNumber; }
    void setInterfaceNumber(int newValue) { interfaceNumber = newValue; }
    void setTransmitAllowed(bool newValue) { allowTransmit = newValue; }


signals:
    void packetReceived(PacketInterface *device, QString data);
    void interfaceChangedState(PacketInterface *iface, PacketInterface::Device_Status state);


public slots:
    
protected:
    virtual void saveSpecificSettings(QSettings&) {}
    virtual void restoreSpecificSettings(QSettings&) {}


    bool allowTransmit;
    bool activateOnStartup;
    int interfaceNumber;
    Device_Status deviceState;
};

#endif // PACKETINTERFACE_H
