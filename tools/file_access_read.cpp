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
#include <unistd.h>

#include <file_access.h>

int main(int argc, char **argv)
{
    int opt;

    std::string outputFile{};

    while ((opt = getopt(argc, argv, "o:")) != -1) {
        switch (opt)
        {
        case 'o':
            outputFile = optarg;
            break;
        case '?':
            std::cerr << "Invalid usage" << std::endl;
            break;
        default:
            break;
        }
    }

    if (optind != argc - 1) {
        std::cerr << "Subdev index missing" << std::endl;
        return -1;
    }

    auto alviumGenCP = AlviumGenCP::open(std::stoi(argv[optind]));
    if (!alviumGenCP)
        return -1;

    auto userDataFile = File::open(*alviumGenCP, FileSelector::UserData, FileOpenMode::Read);
    if (!userDataFile)
        return -1;

    auto const length = userDataFile->length();
    if (length < 0)
        return length;

    auto buffer = std::make_unique<uint8_t[]>(length);

    auto const res = userDataFile->read(buffer.get(), length);
    if (res < 0)
        return res;

    if (!outputFile.empty()) {
        std::fstream stream{outputFile, std::fstream::out | std::fstream::trunc | std::fstream::binary};
        stream.write(reinterpret_cast<char*>(buffer.get()), res);
    } else {
        write(STDOUT_FILENO, buffer.get(), length);
    }


    return 0;
}