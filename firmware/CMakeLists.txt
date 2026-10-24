# Grogu: the controller-stand's ESP32-S3 firmware.
#
#   firmware/build.sh                 build (and sign) the image
#
# Wi-Fi credentials for development come from sdkconfig.secrets, which isn't committed:
# copy sdkconfig.secrets.example and fill it in. Provisioned devices take theirs from the
# prov partition instead.
cmake_minimum_required(VERSION 3.16)

set(IDF_TARGET esp32s3)
if(NOT SDKCONFIG_DEFAULTS)
  set(SDKCONFIG_DEFAULTS "sdkconfig.defaults")
endif()
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/sdkconfig.secrets)
  list(APPEND SDKCONFIG_DEFAULTS "sdkconfig.secrets")
endif()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(grogu)
