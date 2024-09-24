/* alvium file access example - Example tool for accessing user data files in Alvium CSI2 cameras
 * Copyright (C) 2024 Allied Vision Technologies GmbH

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

#include <iostream>

#include <file_access.h>



enum class FileOperation : uint8_t {
    Open,
    Close,
    Read, 
    Write,
    Delete,
};


static const uint32_t FileStatusClosed = 0;
static const uint32_t FileStatusOpen = 0;

static const uint64_t FileAccessBufferAddr = 0xD0004000;
static const uint64_t FileAccessBufferLength = 0x0400;

static const uint64_t StructFileStatusAddr = 0xD0000100;
static const uint64_t StructFileStatusLength = 0x08;

static const uint64_t RegFileOperationExecuteAddr = 0xD0003000;
static const uint64_t RegFileOperationExecuteLength = 0x08;

static const uint64_t RegFileAccessOffsetAddr = 0xD0005000;
static const uint64_t RegFileAccessOffsetLength = 0x4;

static const uint64_t RegFileSizeBaseAddr = 0xD0005300;
static const uint64_t RegFileSizeLength = 0x4;

static const uint64_t RegFileAccessLengthAddr = 0xD0005100;
static const uint64_t RegFileAccessLengthLength = 0x04;

static const uint64_t RegFileSizeMaxAddr = 0xD0005210;
static const uint64_t RegFileSizeMaxLength = 0x4;

struct FileStatus {
    uint16_t open : 1;
    uint16_t : 3;
    uint16_t writeable : 1;
    uint16_t readable : 1;
    uint16_t : 10;
    uint16_t update_status;
    uint32_t selector_open;
} __attribute__((packed));


static int readFileStatus(AlviumGenCP &gencp, FileStatus & status)
{
    int res = gencp.readRegister(StructFileStatusAddr, reinterpret_cast<uint8_t*>(&status), sizeof(status));
    if (res < 0)
        return res;

    return 0;
}

int executeFileOperation(AlviumGenCP &gencp, FileOperation operation, FileSelector selector, std::optional<FileOpenMode> open_mode = std::nullopt)
{
    uint64_t val = uint64_t(operation) | (uint64_t(selector) << 32);

    if (operation == FileOperation::Open) {
        if (!open_mode)
            return -1;

        val |= (uint64_t(*open_mode) << 16);
    }

    return gencp.writeRegister(RegFileOperationExecuteAddr, reinterpret_cast<uint8_t*>(&val), sizeof(val));
}


std::optional<File> File::open(AlviumGenCP &gencp, FileSelector selector, FileOpenMode openMode)
{
    FileStatus status{};

    int res = readFileStatus(gencp, status);
    if (res < 0)
        return std::nullopt;

    if (status.open) {
        res = executeFileOperation(gencp, FileOperation::Close, FileSelector(status.selector_open));
        if (res < 0)
            return std::nullopt;
    }

    res = executeFileOperation(gencp, FileOperation::Open, selector, openMode);
    if (res < 0)
        return std::nullopt;

    res = readFileStatus(gencp, status);
    if (res < 0)
        return std::nullopt;

    if (!status.open)
        return std::nullopt;

    return File{gencp, selector, openMode};
}

int File::remove(AlviumGenCP &gencp, FileSelector selector)
{
    return executeFileOperation(gencp, FileOperation::Delete, selector);
}

File::File(AlviumGenCP &gencp, FileSelector selector, FileOpenMode openMode) : m_gencp{gencp}, m_selector{selector}, m_openMode{openMode}
{
    m_refCount = new size_t{1};
}

File::File(const File &other) :  m_gencp{other.m_gencp}, m_selector{other.m_selector}, m_openMode{other.m_openMode}, m_refCount{other.m_refCount}{
    (*m_refCount)++;
}

File::~File()
{
    (*m_refCount)--;
    if (*m_refCount == 0) {
        executeFileOperation(m_gencp, FileOperation::Close, m_selector);
        delete m_refCount;
    }
}

int File::write(const uint8_t *data, size_t length, bool showProgress)
 {
    if (m_openMode == FileOpenMode::Read)
        return -1;

    if (this->length() != 0) {
        std::cerr << "File exists!!" << std::endl;
        return -1;
    }

    auto const optlen = m_gencp.maxWritePacketPayloadSize();
    if (optlen < 0)
        return optlen;

    uint32_t maxFileLength{};

    int res = m_gencp.readRegister(RegFileSizeMaxAddr, reinterpret_cast<uint8_t*>(&maxFileLength), sizeof(maxFileLength));
    if (res < 0)
        return res;

    if (length > maxFileLength) {
        std::cerr << "Data too large!!" << std::endl;
        return -1;
    }

    uint32_t const chunkSize = std::min(FileAccessBufferLength, uint64_t(optlen));
    uint32_t chunkIdx = 0;
    uint32_t remaining = length;

    while (remaining > 0) {
        uint32_t const bytesToRead = remaining > chunkSize ? chunkSize : remaining;
        uint32_t const offset = chunkIdx * chunkSize;

        if (showProgress) {
            uint32_t const written = length - remaining;
            uint32_t const percent = (100 * written) / length;
            std::cout << "Writing: " << percent << "% (" << written << "/" << length << ")\r" << std::flush;
        }

        int res = m_gencp.writeRegister(RegFileAccessLengthAddr, reinterpret_cast<const uint8_t*>(&bytesToRead), sizeof(bytesToRead));
        if (res < 0)
            return res;

        res = m_gencp.writeRegister(FileAccessBufferAddr, data + offset, bytesToRead);
        if (res < 0)
            return res;

        res = executeFileOperation(m_gencp, FileOperation::Write, m_selector);
        if (res < 0)
            return res;

        remaining -= bytesToRead;
        chunkIdx++;
    }

    if (showProgress) {
        uint32_t const written = length - remaining;
        uint32_t const percent = (100 * written) / length;
        std::cout << "Written: " << percent << "% (" << written << "/" << length << ")" << std::endl;
    }

    return length;
 }

 ssize_t File::read(uint8_t *data, size_t maxLength)
 {
    if (m_openMode == FileOpenMode::Write)
        return -1;

    auto const length = this->length();

    if (length == 0 || length > maxLength)
        return -1;

    auto const optlen = m_gencp.maxReadPacketPayloadSize();
    if (optlen < 0)
        return optlen;

    uint32_t const chunkSize = std::min(FileAccessBufferLength, uint64_t(optlen));
    uint32_t chunkIdx = 0;
    uint32_t remaining = length;

    while (remaining > 0) {
        uint32_t const bytesToRead = remaining > chunkSize ? chunkSize : remaining;
        uint32_t const offset = chunkIdx * chunkSize;

        int res = m_gencp.writeRegister(RegFileAccessLengthAddr,
                                        reinterpret_cast<const uint8_t*>(&bytesToRead),
                                        sizeof(bytesToRead));
        if (res < 0)
            return res;

        res = executeFileOperation(m_gencp, FileOperation::Read, m_selector);
        if (res < 0)
            return res;

        res = m_gencp.readRegister(FileAccessBufferAddr, data + offset, bytesToRead);
        if (res < 0)
            return res;

        remaining -= bytesToRead;
        chunkIdx++;
    }

    return length;
 }

 ssize_t File::length() const
 {
    uint32_t fileLength{};

    int res = m_gencp.readRegister(RegFileSizeBaseAddr + RegFileSizeLength * uint64_t(m_selector),
                                   reinterpret_cast<uint8_t*>(&fileLength), sizeof(fileLength));
    if (res < 0)
        return res;

    return fileLength;
 }