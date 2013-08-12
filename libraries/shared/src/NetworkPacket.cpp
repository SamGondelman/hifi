//
//  NetworkPacket.cpp
//  shared
//
//  Created by Brad Hefta-Gaub on 8/9/13.
//  Copyright (c) 2013 High Fidelity, Inc. All rights reserved.
//
//  A really simple class that stores a network packet between being received and being processed
//

#include <cstring>
#include <QtDebug>

#include "NetworkPacket.h"

NetworkPacket::NetworkPacket()  : _packetLength(0) {
}

NetworkPacket::NetworkPacket(const NetworkPacket& packet) {
    memcpy(&_address, &packet.getAddress(), sizeof(_address));
    _packetLength = packet.getLength();
    memcpy(&_packetData[0], packet.getData(), _packetLength);
}

NetworkPacket::NetworkPacket(sockaddr& address, unsigned char*  packetData, ssize_t packetLength) {
    memcpy(&_address, &address, sizeof(_address));
    _packetLength = packetLength;
    memcpy(&_packetData[0], packetData, packetLength);
};
