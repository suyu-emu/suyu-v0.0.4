# tzdb_to_nx

This is a CMake/C++ project to convert RFC 8536 time zone data to the Nintendo Switch's format.
This makes use a lot of Unix system calls as well as a bash script to convert the data, so it likely requires a bit of work to port to a non-POSIX platform, such as Windows.

Intended for use with the [yuzu Emulator](https://yuzu-emu.org/) project, but the project in the future likely won't ship synthesized Switch archives.
That leaves this project in a place where it is not likely to be used, but will remain here as a reference.

- tzdb: CMake and bash script to build and convert time zone data from https://www.iana.org/time-zones into the Nintendo Switch's format.
- tzdb2nx: C++ program that converts a single tzif file to the Nintendo's format.

The fine folks over at [SwitchBrew](https://switchbrew.org/wiki/PSC_services#ITimeZoneService) have left very helpful information on reading the data.
Nintendo's file is simply the TZif version 2 data, with standard_indicators and ut_indicators data stripped out (and the necessary modifications needed in the header to make the data valid).
This means the TZif 1 data is not present, so essentially we are left with the second half of each file.

Nintendo also does not seem to run the `zic` program on their output when they build the time zone data.
I have left the relevant build command for that in src/tzdb/CMakeLists.txt commented out, but it isn't used here.
This lets the project produce data identical to Nintendo's firmware for time zones, however this code does not produce the time zone data on US/Pacific-New or America/East-Saskatchewan (I may have bunged up the actual paths for these as this is 3 day old memory).

The CMake and C++ code in this repository is licensed under the MIT License.
The source files date.c, newstrftime.3 and strftime.c from submodule eggert/tz use the BSD-3 clause license [[source]](https://github.com/eggert/tz/blob/main/LICENSE).
The time zone data output from this repository, like those found in archives in the Release setcion, is in the public domain [[source]](https://github.com/eggert/tz/blob/main/LICENSE).
