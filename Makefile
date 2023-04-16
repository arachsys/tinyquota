BINDIR := $(PREFIX)/bin
CFLAGS := -Os -Wall -Wfatal-errors

%:: %.c Makefile
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^)

all: quotaget quotaset

install: quotaget quotaset
	mkdir -p $(DESTDIR)$(BINDIR)
	install -s $^ $(DESTDIR)$(BINDIR)

clean:
	rm -f quotaget quotaset

.PHONY: all install clean
