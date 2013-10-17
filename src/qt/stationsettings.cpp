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

#include "stationsettings.h"

StationSettings::StationSettings(QObject *parent) :
    QObject(parent)
{
}

void StationSettings::saveSettings(QSettings &settings)
{
    settings.setValue("callsign", callsign());
    settings.setValue("lat", lat());
    settings.setValue("lon", lon());
    settings.setValue("hasPHG", hasPHGD());
    settings.setValue("comment", comment());
    settings.setValue("group", QChar(group()));
    settings.setValue("symbol", QChar(symbol()));
}


void StationSettings::restoreFromSettings(QSettings &settings)
{
    setCallsign(settings.value("callsign").toString());
    setLat(settings.value("lat").toFloat());
    setLon(settings.value("lon").toFloat());
    sethasPHGD(settings.value("hasPHG").toBool());
    setComment(settings.value("comment").toString());
    setGroup(settings.value("group").toChar().cell());
    setSymbol(settings.value("symbol").toChar().cell());
}
