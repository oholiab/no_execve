CC=gcc
CFLAGS=-shared -fPIC

default: fexecve_preload.so

%.so: %.c
	$(CC) $(CFLAGS) -o $@ $<
