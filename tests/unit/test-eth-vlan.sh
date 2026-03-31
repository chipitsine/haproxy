#!/bin/sh

check() {
	:
}

run() {
	gcc -Iinclude -Wall -W -fomit-frame-pointer -Os \
		${ROOTDIR}/tests/unit/test-eth-vlan.c \
		-o ${ROOTDIR}/tests/unit/test-eth-vlanOs
	${ROOTDIR}/tests/unit/test-eth-vlanOs
	rm ${ROOTDIR}/tests/unit/test-eth-vlanOs

	gcc -Iinclude -Wall -W -fomit-frame-pointer -O2 \
		${ROOTDIR}/tests/unit/test-eth-vlan.c \
		-o ${ROOTDIR}/tests/unit/test-eth-vlanO2
	${ROOTDIR}/tests/unit/test-eth-vlanO2
	rm ${ROOTDIR}/tests/unit/test-eth-vlanO2
}

case "$1" in
	"check")
		check
	;;
	"run")
		run
	;;
esac
