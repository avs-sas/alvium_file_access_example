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
#include <fstream>
#include <filesystem>

#include <cstring>
#include <file_access.h>



int main(int argc, char *argv[])
{
    if (argc != 3) {
        std::cerr << "Invalid usage!" << std::endl;
        return -1;
    }



    auto const inputFilePath = std::filesystem::path{argv[2]};

    if (!std::filesystem::exists(inputFilePath))
        return -1;

    auto alviumGenCP = AlviumGenCP::open(std::stoi(argv[1]));

    auto const currentLength = [&]() -> int {
        auto userDataFileRead = File::open(*alviumGenCP, FileSelector::UserData, FileOpenMode::Read);
        if (!userDataFileRead) {
            std::cerr << "Open failed" << std::endl;
            return -1;
        }

        return userDataFileRead->length();
    }();

    if (currentLength < 0) {
        return -1;
    }

    if (currentLength > 0) {
        auto const  res = File::remove(*alviumGenCP, FileSelector::UserData);
        if (res < 0) {
            std::cerr << "File remove failed" << std::endl;
            return res;
        }
    }

    auto userDataFile = File::open(*alviumGenCP, FileSelector::UserData, FileOpenMode::Write);
    if (!userDataFile) {
        std::cerr << "Open failed" << std::endl;
        return -1;
    }

    auto const length = std::filesystem::file_size(inputFilePath);
    if (length == 0) {
        std::cerr << "File to write is empty" << std::endl;
        return -1;
    }

    std::cout << "File length: " << length << std::endl;
    auto buffer = std::make_unique<uint8_t[]>(length);

    std::fstream stream{inputFilePath, std::fstream::in | std::fstream::binary};
    stream.read(reinterpret_cast<char*>(buffer.get()), length);

    auto const res = userDataFile->write(buffer.get(), length, true);
    if (res < 0)
        return res;

    return 0;
}