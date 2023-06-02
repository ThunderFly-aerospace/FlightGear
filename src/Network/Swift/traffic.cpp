/*
 * Traffic module for swift<->FG connection
 * SPDX-FileCopyrightText: (C) 2019-2022 swift Project Community / Contributors (https://swift-project.org/)
 * SPDX-FileCopyrightText: (C) 2019-2022 Lars Toenning <dev@ltoenning.de>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

//! \cond PRIVATE

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "traffic.h"
#include "SwiftAircraftManager.h"

#include <algorithm>
#include <iostream>


// clazy:excludeall=reserve-candidates

namespace FGSwiftBus {

CTraffic::CTraffic()
{
    SG_LOG(SG_NETWORK, SG_INFO, "FGSwiftBus Traffic started");
}

CTraffic::~CTraffic()
{
    cleanup();
    SG_LOG(SG_NETWORK, SG_INFO, "FGSwiftBus Traffic stopped");
}

const std::string& CTraffic::InterfaceName()
{
    static std::string s(FGSWIFTBUS_TRAFFIC_INTERFACENAME);
    return s;
}

const std::string& CTraffic::ObjectPath()
{
    static std::string s(FGSWIFTBUS_TRAFFIC_OBJECTPATH);
    return s;
}

bool CTraffic::initialize()
{
    acm.reset(new FGSwiftAircraftManager());
    return acm->isInitialized();
}

void CTraffic::emitSimFrame()
{
    if (m_emitSimFrame) { sendDBusSignal("simFrame"); }
    m_emitSimFrame = !m_emitSimFrame;
}

void CTraffic::emitPlaneAdded(const std::string& callsign)
{
    CDBusMessage signalPlaneAdded = CDBusMessage::createSignal(FGSWIFTBUS_TRAFFIC_OBJECTPATH, FGSWIFTBUS_TRAFFIC_INTERFACENAME, "remoteAircraftAdded");
    signalPlaneAdded.beginArgumentWrite();
    signalPlaneAdded.appendArgument(callsign);
    sendDBusMessage(signalPlaneAdded);
}

void CTraffic::cleanup()
{
    acm.reset();
}

void CTraffic::dbusDisconnectedHandler()
{
    if (acm)
        acm->removeAllPlanes();
}

const char* introspection_traffic = DBUS_INTROSPECT_1_0_XML_DOCTYPE_DECL_NODE;

DBusHandlerResult CTraffic::dbusMessageHandler(const CDBusMessage& message_)
{
    CDBusMessage message(message_);
    const std::string sender = message.getSender();
    const dbus_uint32_t serial = message.getSerial();
    const bool wantsReply = message.wantsReply();

    if (message.getInterfaceName() == DBUS_INTERFACE_INTROSPECTABLE) {
        if (message.getMethodName() == "Introspect") {
            sendDBusReply(sender, serial, introspection_traffic);
        }
    } else if (message.getInterfaceName() == FGSWIFTBUS_TRAFFIC_INTERFACENAME) {
        if (message.getMethodName() == "acquireMultiplayerPlanes") {
            queueDBusCall([=]() {
                std::string owner;
                bool acquired = true;
                CDBusMessage reply = CDBusMessage::createReply(sender, serial);
                reply.beginArgumentWrite();
                reply.appendArgument(acquired);
                reply.appendArgument(owner);
                sendDBusMessage(reply);
            });
        } else if (message.getMethodName() == "initialize") {
            sendDBusReply(sender, serial, initialize());
        } else if (message.getMethodName() == "cleanup") {
            maybeSendEmptyDBusReply(wantsReply, sender, serial);
            queueDBusCall([=]() {
                cleanup();
            });
        } else if (message.getMethodName() == "addPlane") {
            maybeSendEmptyDBusReply(wantsReply, sender, serial);
            std::string callsign;
            std::string modelName;
            std::string aircraftIcao;
            std::string airlineIcao;
            std::string livery;
            message.beginArgumentRead();
            message.getArgument(callsign);
            message.getArgument(modelName);
            message.getArgument(aircraftIcao);
            message.getArgument(airlineIcao);
            message.getArgument(livery);

            queueDBusCall([=]() {
                if (acm->addPlane(callsign, modelName)) {
                    emitPlaneAdded(callsign);
                }
            });
        } else if (message.getMethodName() == "removePlane") {
            maybeSendEmptyDBusReply(wantsReply, sender, serial);
            std::string callsign;
            message.beginArgumentRead();
            message.getArgument(callsign);
            queueDBusCall([=]() {
                acm->removePlane(callsign);
            });
        } else if (message.getMethodName() == "removeAllPlanes") {
            maybeSendEmptyDBusReply(wantsReply, sender, serial);
            queueDBusCall([=]() {
                acm->removeAllPlanes();
            });
        } else if (message.getMethodName() == "setPlanesPositions") {
            maybeSendEmptyDBusReply(wantsReply, sender, serial);
            std::vector<std::string> callsigns;
            std::vector<double> latitudes;
            std::vector<double> longitudes;
            std::vector<double> altitudes;
            std::vector<double> pitches;
            std::vector<double> rolls;
            std::vector<double> headings;
            std::vector<double> groundspeeds;
            std::vector<bool> onGrounds;
            message.beginArgumentRead();
            message.getArgument(callsigns);
            message.getArgument(latitudes);
            message.getArgument(longitudes);
            message.getArgument(altitudes);
            message.getArgument(pitches);
            message.getArgument(rolls);
            message.getArgument(headings);
            message.getArgument(groundspeeds);
            message.getArgument(onGrounds);
            queueDBusCall([=]() {
                std::vector<SwiftPlaneUpdate> updates;
                for (long unsigned int i = 0; i < latitudes.size(); i++) {
                    SGGeod pos;
                    pos.setLatitudeDeg(latitudes.at(i));
                    pos.setLongitudeDeg(longitudes.at(i));
                    pos.setElevationFt(altitudes.at(i));
                    SGVec3d orientation(pitches.at(i), rolls.at(i), headings.at(i));
                    updates.push_back({callsigns.at(i), pos, orientation, groundspeeds.at(i), onGrounds.at(i)});
                }
                acm->updatePlanes(updates);
            });
        } else if (message.getMethodName() == "getRemoteAircraftData") {
            std::vector<std::string> requestedcallsigns;
            message.beginArgumentRead();
            message.getArgument(requestedcallsigns);
            queueDBusCall([=]() {
                std::vector<std::string> callsigns = requestedcallsigns;
                std::vector<double> latitudesDeg;
                std::vector<double> longitudesDeg;
                std::vector<double> elevationsM;
                std::vector<double> verticalOffsets;
                acm->getRemoteAircraftData(callsigns, latitudesDeg, longitudesDeg, elevationsM, verticalOffsets);
                CDBusMessage reply = CDBusMessage::createReply(sender, serial);
                reply.beginArgumentWrite();
                reply.appendArgument(callsigns);
                reply.appendArgument(latitudesDeg);
                reply.appendArgument(longitudesDeg);
                reply.appendArgument(elevationsM);
                reply.appendArgument(verticalOffsets);
                sendDBusMessage(reply);
            });
        } else if (message.getMethodName() == "getElevationAtPosition") {
            std::string callsign;
            double latitudeDeg;
            double longitudeDeg;
            double altitudeMeters;
            message.beginArgumentRead();
            message.getArgument(callsign);
            message.getArgument(latitudeDeg);
            message.getArgument(longitudeDeg);
            message.getArgument(altitudeMeters);
            queueDBusCall([=]() {
                SGGeod pos;
                pos.setLatitudeDeg(latitudeDeg);
                pos.setLongitudeDeg(longitudeDeg);
                pos.setElevationM(altitudeMeters);
                double elevation = acm->getElevationAtPosition(callsign, pos);
                CDBusMessage reply = CDBusMessage::createReply(sender, serial);
                reply.beginArgumentWrite();
                reply.appendArgument(callsign);
                reply.appendArgument(elevation);
                sendDBusMessage(reply);
            });
        } else if (message.getMethodName() == "setPlanesTransponders") {
            maybeSendEmptyDBusReply(wantsReply, sender, serial);
            std::vector<std::string> callsigns;
            std::vector<int> codes;
            std::vector<bool> modeCs;
            std::vector<bool> idents;
            message.beginArgumentRead();
            message.getArgument(callsigns);
            message.getArgument(codes);
            message.getArgument(modeCs);
            message.getArgument(idents);
            std::vector<AircraftTransponder> transponders;
            transponders.reserve(callsigns.size());
            for (long unsigned int i = 0; i < callsigns.size(); i++) {
                transponders.emplace_back(callsigns.at(i), codes.at(i), modeCs.at(i), idents.at(i));
            }
            queueDBusCall([=]() {
                acm->setPlanesTransponders(transponders);
            });
        } else if (message.getMethodName() == "setPlanesSurfaces") {
            maybeSendEmptyDBusReply(wantsReply, sender, serial);
            std::vector<std::string> callsigns;
            std::vector<double> gears;
            std::vector<double> flaps;
            std::vector<double> spoilers;
            std::vector<double> speedBrakes;
            std::vector<double> slats;
            std::vector<double> wingSweeps;
            std::vector<double> thrusts;
            std::vector<double> elevators;
            std::vector<double> rudders;
            std::vector<double> ailerons;
            std::vector<bool> landLights;
            std::vector<bool> taxiLights;
            std::vector<bool> beaconLights;
            std::vector<bool> strobeLights;
            std::vector<bool> navLights;
            std::vector<int> lightPatterns;
            message.beginArgumentRead();
            message.getArgument(callsigns);
            message.getArgument(gears);
            message.getArgument(flaps);
            message.getArgument(spoilers);
            message.getArgument(speedBrakes);
            message.getArgument(slats);
            message.getArgument(wingSweeps);
            message.getArgument(thrusts);
            message.getArgument(elevators);
            message.getArgument(rudders);
            message.getArgument(ailerons);
            message.getArgument(landLights);
            message.getArgument(taxiLights);
            message.getArgument(beaconLights);
            message.getArgument(strobeLights);
            message.getArgument(navLights);
            message.getArgument(lightPatterns);
            std::vector<AircraftSurfaces> surfaces;
            surfaces.reserve(callsigns.size());
            for (long unsigned int i = 0; i < callsigns.size(); i++) {
                surfaces.emplace_back(callsigns.at(i), gears.at(i), flaps.at(i), spoilers.at(i), speedBrakes.at(i), slats.at(i),
                                      wingSweeps.at(i), thrusts.at(i), elevators.at(i), rudders.at(i), ailerons.at(i),
                                      landLights.at(i), taxiLights.at(i), beaconLights.at(i), strobeLights.at(i), navLights.at(i), lightPatterns.at(i));
            }
            queueDBusCall([=]() {
                acm->setPlanesSurfaces(surfaces);
            });
        } else {
            // Unknown message. Tell DBus that we cannot handle it
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }
    }
    return DBUS_HANDLER_RESULT_HANDLED;
}

int CTraffic::process()
{
    invokeQueuedDBusCalls();
    return 1;
}

} // namespace FGSwiftBus

//! \endcond
