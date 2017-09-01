#!/usr/bin/env sh

# Exit on errors
set -e

cd "${TRAVIS_BUILD_DIR}"

. travis-ci/defs.sh

print_headline "Configuring for building for ${QT} on ${TRAVIS_OS_NAME}"

if [ "${QT}" = "qt5" ]; then
	export DISPLAY=:99.0
	sh -e /etc/init.d/xvfb start
	sleep 3 # give xvfb some time to start
	qmake texstudio.pro CONFIG+=debug
elif [ $QT = qt5NoPoppler ]; then
	qmake texstudio.pro NO_POPPLER_PREVIEW=1
elif [ $QT = qt4 ]; then
	qmake texstudio.pro 
elif [ "${TRAVIS_OS_NAME}" = "osx" ]; then
	qmake texstudio.pro CONFIG-=debug
fi

cd "${TRAVIS_BUILD_DIR}"

print_info "Successfully configured build"

