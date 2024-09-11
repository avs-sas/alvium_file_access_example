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


#include <gencp.h>


enum class FileSelector : uint32_t {
    UserData = 0x11
};

enum class FileOpenMode : uint8_t {
    Read = 1,
    Write = 2,
};

class File {
public:
    static std::optional<File> open(AlviumGenCP &gencp, FileSelector selector, FileOpenMode openMode);
    static int remove(AlviumGenCP &gencp, FileSelector selector);

    File(const File &other);

    ~File();

    int write(const uint8_t *data, size_t length, bool showProgress = false);
    ssize_t read(uint8_t *data, size_t maxLength);

    ssize_t length() const;
private:
    File(AlviumGenCP &gencp, FileSelector selector, FileOpenMode openMode);

    const FileSelector m_selector;
    const FileOpenMode m_openMode;
    AlviumGenCP &m_gencp;
    size_t *m_refCount;
};
