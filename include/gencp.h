/* alvium file access example - Example tool for accessing user data files in Alvium CSI2 cameras
 * Copyright (C) 2024 Allied Vision Technologies Gmbh

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#pragma once

#include <memory>
#include <optional>
#include <array>

#include <sys/types.h>

class AlviumGenCP {
public:
    static std::optional<AlviumGenCP> open(int subdev);

    int writeRegister(uint64_t addr, const uint8_t *buffer, size_t length);
    int readRegister(uint64_t addr, uint8_t *buffer, size_t length); 

    size_t maxPacketSize() const;
    size_t maxReadPacketPayloadSize() const;
    size_t maxWritePacketPayloadSize() const;
private:
    AlviumGenCP(int transferFd, int subdev, std::array<uint16_t, 3> m_addr);

    int writePaket(const void *paket, size_t length);
    int readPaket(void *paket, size_t length);

    int writeRaw(uint16_t addr, const uint8_t *buffer, size_t length) const;
    int readRaw(uint16_t addr, uint8_t *buffer, size_t length) const;

    const int m_transferFd;
    const int m_subdev;
    const std::array<uint16_t, 3> m_addr;

    uint16_t m_requestId{1};
};


