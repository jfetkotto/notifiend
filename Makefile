all:
	gcc `pkg-config --cflags --libs libnotify libmpdclient` -std=c99 -Wall -Werror -pedantic -o notifiend notifiend.c

debug:
	gcc -g `pkg-config --cflags --libs libnotify libmpdclient` -std=c99 -Wall -pedantic -o notifiend notifiend.c

clean:
	rm notifiend

install:
	mv notifiend ~/.bin/notifiend


