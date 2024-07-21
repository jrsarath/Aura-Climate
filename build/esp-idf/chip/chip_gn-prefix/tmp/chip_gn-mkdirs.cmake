# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/Users/sarath/.espressif/esp-matter/connectedhomeip/connectedhomeip")
  file(MAKE_DIRECTORY "/Users/sarath/.espressif/esp-matter/connectedhomeip/connectedhomeip")
endif()
file(MAKE_DIRECTORY
  "/Users/sarath/Documents/GitHub/Aura/devices/climate/firmware/build/esp-idf/chip"
  "/Users/sarath/Documents/GitHub/Aura/devices/climate/firmware/build/esp-idf/chip/chip_gn-prefix"
  "/Users/sarath/Documents/GitHub/Aura/devices/climate/firmware/build/esp-idf/chip/chip_gn-prefix/tmp"
  "/Users/sarath/Documents/GitHub/Aura/devices/climate/firmware/build/esp-idf/chip/chip_gn-prefix/src/chip_gn-stamp"
  "/Users/sarath/Documents/GitHub/Aura/devices/climate/firmware/build/esp-idf/chip/chip_gn-prefix/src"
  "/Users/sarath/Documents/GitHub/Aura/devices/climate/firmware/build/esp-idf/chip/chip_gn-prefix/src/chip_gn-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/sarath/Documents/GitHub/Aura/devices/climate/firmware/build/esp-idf/chip/chip_gn-prefix/src/chip_gn-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/sarath/Documents/GitHub/Aura/devices/climate/firmware/build/esp-idf/chip/chip_gn-prefix/src/chip_gn-stamp${cfgdir}") # cfgdir has leading slash
endif()
