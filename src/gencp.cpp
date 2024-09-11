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


#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

#include <cstring>

#include <fcntl.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <cppcrc.h>

#include <gencp.h>

namespace fs = std::filesystem;

struct avt3_fw_transfer {
    __u16 addr;
    __u16 len;
    __u8  rd;
    __u8  reserved[3];
} __attribute__((packed));

struct GenCPPrefix {
    const uint16_t preamble{0x0100};
    uint32_t crc;
    const uint16_t channel_id{0x0};
} __attribute__((packed));

struct GenCPCCD {
    union {
        uint16_t flags;
        uint16_t status_code;
    };
    uint16_t command_id;
    uint16_t length;
    uint16_t request_id;
} __attribute__((packed));

struct GenCPReadMemCmd {
    uint64_t register_address;
    uint16_t reserved;
    uint16_t read_length;
} __attribute__((packed));

using GenCPReadMemAck = uint8_t[];

struct GenCPWriteMemCmd {
    uint64_t register_address;
    uint8_t data[];
} __attribute__((packed));

struct GenCPWriteMemAck {
    uint16_t reserved;
    uint16_t length_written;
} __attribute__((packed));

struct GenCPPendingAck {
    uint16_t reserved;
    uint16_t timeout;
} __attribute__((packed));


template<typename SCD>
struct GenCPPaket {
    GenCPPrefix prefix;
    GenCPCCD ccd;
    SCD scd;

    void calcCRC() {
        auto start = reinterpret_cast<const uint8_t*>(&prefix.channel_id);
        auto const size = sizeof(prefix.channel_id) + sizeof(ccd) + ccd.length;
        prefix.crc = CRC32::JAMCRC::calc(start, size);
    }
} __attribute__((packed));


static const fs::path v4l2_sysfs_base{"/sys/class/video4linux/"};

template<class T,typename... Args>
static inline std::unique_ptr<T> makeDynamicStructUniquePtr(size_t arrayLength, Args... args) 
{
    return std::unique_ptr<T>{new (malloc(sizeof(T) + arrayLength)) T(args...)};
}

static int readRawInternal(int fd, uint16_t addr, uint8_t *buffer, size_t length)
{
    avt3_fw_transfer xfer{};
    xfer.addr = addr;
    xfer.len = length;
    xfer.rd = true;

    if (::pwrite(fd, &xfer, sizeof(xfer), 0) < 0)
        return errno;

    if (::pread(fd, buffer, length, 0) < 0)
        return errno;

    return 0;
}


std::optional<AlviumGenCP> AlviumGenCP::open(int subdev)
{
    auto const subdevName = "v4l-subdev" + std::to_string(subdev);
    auto const subdevSysfsPath = v4l2_sysfs_base / subdevName;
    auto const deviceSysfsPath = subdevSysfsPath / "device";

    if (!fs::exists(deviceSysfsPath)) {
        std::cerr << "Failed to get device sysfs path. Tried: " << deviceSysfsPath << std::endl;
        return std::nullopt;
    }

    auto const fwTransferSysfsPath = deviceSysfsPath / "fw_transfer";

    if (!fs::exists(fwTransferSysfsPath)) {
        std::cerr << "Failed to find fw_transfer attribute. Tried: " << fwTransferSysfsPath << std::endl;
        return std::nullopt;
    }

    auto const modeSysfsPath = deviceSysfsPath / "mode";

    if (!fs::exists(modeSysfsPath)) {
        std::cerr << "Failed to find mode attribute. Tried: " << modeSysfsPath << std::endl;
        return std::nullopt;
    }

    std::fstream modeStream{modeSysfsPath, std::fstream::in | std::fstream::out};

    if (!modeStream.is_open()) {
        return std::nullopt;
    }

    modeStream << "gencp";

    auto const transferFd = ::open(fwTransferSysfsPath.c_str(), O_RDWR);

    if (transferFd < 0) {
        return std::nullopt;
    }

    uint16_t tmp{};
    std::array<uint16_t, 3> addr;

    if (readRawInternal(transferFd, 0x10, reinterpret_cast<uint8_t*>(&tmp), sizeof(tmp)) < 0) 
        return std::nullopt;

    addr[0] = be16toh(tmp);

    if (readRawInternal(transferFd, addr[0] + 0xC, reinterpret_cast<uint8_t*>(&tmp), sizeof(tmp)) < 0) 
        return std::nullopt;

    addr[1] = be16toh(tmp);

    if (readRawInternal(transferFd, addr[0] + 0x4, reinterpret_cast<uint8_t*>(&tmp), sizeof(tmp)) < 0) 
        return std::nullopt;

    addr[2] = be16toh(tmp);

    return AlviumGenCP(transferFd, subdev, addr);
}


AlviumGenCP::AlviumGenCP(int transferFd, int subdev, std::array<uint16_t, 3> addr) 
    : m_transferFd{transferFd}, m_subdev{subdev}, m_addr{addr}
{

}


int AlviumGenCP::writeRaw(uint16_t addr, const uint8_t *buffer, size_t length) const
{
    auto tmpSize = sizeof(avt3_fw_transfer) + length;
    auto tmp = std::make_unique<uint8_t[]>(tmpSize);

    auto xfer = reinterpret_cast<avt3_fw_transfer*>(tmp.get());
    xfer->addr = addr;
    xfer->len = length;
    xfer->rd = false;
    memcpy(tmp.get() + sizeof(avt3_fw_transfer), buffer, length);

    if (::pwrite(m_transferFd, tmp.get(), tmpSize, 0) < 0)
        return errno;
    
    return 0;
}

int AlviumGenCP::readRaw(uint16_t addr, uint8_t *buffer, size_t length) const
{
    return readRawInternal(m_transferFd, addr, buffer, length);
}

int AlviumGenCP::writePaket(const void *paket, size_t length)
{
    uint8_t tmp8 = -1;
    int res = 0;

    do {
        res = readRaw(m_addr[0] + 0x18, &tmp8, sizeof(tmp8));
        if (res < 0)
            return res;
    } while(tmp8 != 0);

    res = writeRaw(m_addr[2], reinterpret_cast<const uint8_t*>(paket), length);
    if (res < 0) 
        return res;

    uint16_t const tmp16 = htobe16(length);
    res = writeRaw(m_addr[0] + 0x20, reinterpret_cast<const uint8_t*>(&tmp16), sizeof(tmp16));
    if (res < 0) 
        return res;

    tmp8 = 1;

    res = writeRaw(m_addr[0] + 0x18, &tmp8, sizeof(tmp8));
    if (res < 0) 
        return res;

    do {
        res = readRaw(m_addr[0] + 0x18, &tmp8, sizeof(tmp8));
        if (res < 0)
            return res;
    } while(tmp8 != 2);

    tmp8 = 0;

    res = writeRaw(m_addr[0] + 0x18, &tmp8, sizeof(tmp8));
    if (res < 0) 
        return res;

    return 0;
}

int AlviumGenCP::readPaket(void *paket, size_t length) 
{
    uint8_t tmp8 = -1;
    int res = 0;

    while (1) {
        res = readRaw(m_addr[0] + 0x1C, &tmp8, sizeof(tmp8));
        if (res < 0)
            return res;
        
        if (tmp8 != 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        } else {
            break;
        }
    } 

    uint16_t tmp16{};

    res = readRaw(m_addr[0] + 0x24, reinterpret_cast<uint8_t*>(&tmp16), sizeof(tmp16));

    tmp16 = be16toh(tmp16);

    if (res < 0)
        return res;

    if (tmp16 > length)
        return -1;

    res = readRaw(m_addr[1], reinterpret_cast<uint8_t*>(paket), tmp16);
    if (res < 0)
        return res;

    tmp8 = 2;

    res = writeRaw(m_addr[0] + 0x1C, &tmp8, sizeof(tmp8));
    if (res < 0) 
        return res;

    while (1) {
        res = readRaw(m_addr[0] + 0x1C, &tmp8, sizeof(tmp8));
         
        if (tmp8 != 2) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        } else {
            break;
        }
    }

    tmp8 = 0;

    res = writeRaw(m_addr[0] + 0x1C, &tmp8, sizeof(tmp8));
    if (res < 0) 
        return res;

    return 0;
}

int AlviumGenCP::writeRegister(uint64_t addr, const uint8_t *buffer, size_t length)
{
    auto const maxWriteDataSize = maxWritePacketPayloadSize();
    size_t remaining = length;
    size_t currentChunk = 0;

    while (remaining > 0) {
        auto const bytesToRead = remaining > maxWriteDataSize ? maxWriteDataSize : remaining;
        auto const offset = currentChunk * maxWriteDataSize;

        auto const cmdLength = sizeof(GenCPPaket<GenCPWriteMemCmd>) + bytesToRead;
           
        auto cmd = makeDynamicStructUniquePtr<GenCPPaket<GenCPWriteMemCmd>>(bytesToRead);
        
        cmd->scd.register_address = addr + offset;
        memcpy(cmd->scd.data, buffer + offset, bytesToRead);

        cmd->ccd.flags = (1 << 14);
        cmd->ccd.command_id = 0x0802;
        cmd->ccd.length = sizeof(cmd->scd) + bytesToRead;
        cmd->ccd.request_id = m_requestId;
        
        cmd->calcCRC();

        int res = writePaket(cmd.get(), cmdLength);
        if (res < 0)
            return res;

        GenCPPaket<GenCPWriteMemAck> ack{};

        do {
            memset(&ack, 0, sizeof(ack));

            res = readPaket(&ack, sizeof(ack));
            if (res < 0)
                return res;

            if (ack.ccd.command_id == 0x0805) {
                auto const pendingAck = reinterpret_cast<GenCPPendingAck*>(&ack.scd);
                std::this_thread::sleep_for(std::chrono::milliseconds(pendingAck->timeout));
            }
            
        } while (ack.ccd.command_id == 0x805);

        
        if(ack.ccd.command_id != 0x0803) 
            return -1;
        
        if(ack.ccd.status_code != 0x0) 
            return -1;

        if (ack.ccd.request_id != m_requestId)
            return -1;

        m_requestId++;
        if (m_requestId == 0)
            m_requestId = 1;

        
        currentChunk++;
        remaining -= bytesToRead;
    }

    return 0;
}

int AlviumGenCP::readRegister(uint64_t addr, uint8_t *buffer, size_t length)
{

    size_t remaining = length;
    size_t currentChunk = 0;

    auto const maxReadDataSize = maxReadPacketPayloadSize();

    while (remaining > 0) {
        auto const bytesToRead = remaining > maxReadDataSize ? maxReadDataSize : remaining;
        auto const offset = currentChunk * maxReadDataSize;

        GenCPPaket<GenCPReadMemCmd> cmd{};
        cmd.scd.register_address = addr + offset;
        cmd.scd.read_length = bytesToRead;

        cmd.ccd.flags = (1 << 14);
        cmd.ccd.command_id = 0x0800;
        cmd.ccd.length = sizeof(cmd.scd);
        cmd.ccd.request_id = 0;
        
        cmd.calcCRC();

        int res = writePaket(&cmd, sizeof(cmd));
        if (res < 0)
            return res;

        auto const ackLength = sizeof(GenCPPaket<GenCPReadMemAck>) + bytesToRead;

        auto ack = makeDynamicStructUniquePtr<GenCPPaket<GenCPReadMemAck>>(bytesToRead);

        res = readPaket(ack.get(), ackLength);
        if (res < 0)
            return res;

        if(ack->ccd.command_id != 0x0801) 
            return -1;
        
        if(ack->ccd.status_code != 0x0) 
            return -1;
        
        memcpy(buffer + offset, &ack->scd[0], bytesToRead);

        currentChunk++;
        remaining -= bytesToRead;
    }

    return 0;
}


size_t AlviumGenCP::maxPacketSize() const
{
    struct stat stat{};

    fstat(m_transferFd, &stat);

    return std::min(stat.st_size - sizeof(avt3_fw_transfer), 1024UL);
}

size_t AlviumGenCP::maxReadPacketPayloadSize() const
{
    return maxPacketSize() - sizeof(GenCPPaket<GenCPReadMemAck>);
}

size_t AlviumGenCP::maxWritePacketPayloadSize() const
{
    return maxPacketSize() - sizeof(GenCPPaket<GenCPWriteMemCmd>);
}