#!/bin/bash

PROJ_ROOT='../'
DEVICE_TYPE='pnu'
VERSION=$(head -n1 ${PROJ_ROOT}/version.txt)
BIN='pnu.bin'

#mender-artifact write rootfs-image
#mender-artifact write rootfs-imag --compression none --device-type ${DEVICE_TYPE} 

mender-artifact write rootfs-image --compression none --device-type ${DEVICE_TYPE} --artifact-name ${DEVICE_TYPE}-v${VERSION} --output-path ${DEVICE_TYPE}-v${VERSION}.mender --file ${PROJ_ROOT}/build/${BIN}

# from example
# -compression none --device-type mender-esp32-example --artifact-name mender-esp32-example-v$(head -n1 path/to/mender-esp32-example/VERSION.txt) --output-path path/to/mender-esp32-example/build/mender-esp32-example-v$(head -n1 path/to/mender-esp32-example/VERSION.txt).mender --file path/to/mender-esp32-example/build/mender-esp32-example.bin
