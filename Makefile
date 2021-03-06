PREFIX=/usr/${EXTRA_PREFIX}
PLUGINDIR=${PREFIX}/lib/collectd
INCLUDEDIR=/usr/include/collectd/ ${EXTRA_INCLUDE}

CFLAGS=-I${INCLUDEDIR} -Wall -Werror -g -O2 

bin:
	${CC} -DHAVE_CONFIG_H ${CFLAGS} -c nagiostats.c  -fPIC -DPIC -o nagiostats.o
	${CC} -shared nagiostats.o -Wl,-soname -Wl,nagiostats.so -o nagiostats.so

clean:
	rm -f nagiostats.o nagiostats.so

install:
	mkdir -p ${DESTDIR}/${PLUGINDIR}/
	cp nagiostats.so ${DESTDIR}/${PLUGINDIR}/
