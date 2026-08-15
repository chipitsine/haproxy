#!/bin/sh

check() {
	${HAPROXY_PROGRAM} -vv | grep CFLAGS | grep -q "fsanitize=address"
}

run() {
	${HAPROXY_PROGRAM} -dI -f ${ROOTDIR}/.github/h2spec.config -c
	${HAPROXY_PROGRAM} -dI -f ${ROOTDIR}/examples/content-sw-sample.cfg -c
	${HAPROXY_PROGRAM} -dI -f ${ROOTDIR}/examples/option-http_proxy.cfg -c
	${HAPROXY_PROGRAM} -dI -f ${ROOTDIR}/examples/quick-test.cfg -c
	${HAPROXY_PROGRAM} -dI -f ${ROOTDIR}/examples/transparent_proxy.cfg -c

	#${HAPROXY_PROGRAM} -dI -f ${ROOTDIR}/examples/basic-config-edge.cfg -c
	${HAPROXY_PROGRAM} -dI -f ${ROOTDIR}/examples/socks4.cfg -c
	${HAPROXY_PROGRAM} -dI -f ${ROOTDIR}/examples/wurfl-example.cfg -c
	
    #${HAPROXY_PROGRAM} -dI -f ${ROOTDIR}/examples/traces.cfg -c 
#  [ALERT]    (10629) : config : parsing [/home/runner/work/haproxy/haproxy/examples/traces.cfg:17] : No such trace source 'h3'
#  [ALERT]    (10629) : config : parsing [/home/runner/work/haproxy/haproxy/examples/traces.cfg:18] : No such trace source 'quic'
#  [ALERT]    (10629) : config : parsing [/home/runner/work/haproxy/haproxy/examples/traces.cfg:19] : No such trace source 'qcm'

  
	#${HAPROXY_PROGRAM} -dI -f ${ROOTDIR}/examples/keylog-test.cfg -c
    ${HAPROXY_PROGRAM} -dI -f ${ROOTDIR}/examples/mptcp.cfg -c
    
	${HAPROXY_PROGRAM} -dI -f ${ROOTDIR}/examples/games.cfg -c
	
}

case "$1" in
	"check")
		check
	;;
	"run")
		run
	;;
esac
