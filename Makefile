DESTDIR ?= /usr/
CFLAGS = -c -Wall -DWNCK_I_KNOW_THIS_IS_UNSTABLE

PROGNAME = xfce4-windowheader-plugin

$(PROGNAME): main.o libxfce4panel.mk libxfcegui4.mk libwnck.mk
	gcc -o $(PROGNAME) main.o $(LIBS)

%.o: %.c
	gcc $(CFLAGS) -o $@ $<

install: $(PROGNAME)
	install -m 755 $(PROGNAME) $(DESTDIR)/lib/xfce4/panel-plugins/
	install -m 644 windowheader.desktop $(DESTDIR)/share/xfce4/panel-plugins/

libxfce4panel.mk:
	@echo CFLAGS += $(shell pkg-config --cflags libxfce4panel-1.0) >> $@
	@echo LIBS += $(shell pkg-config --libs libxfce4panel-1.0) >> $@

libxfcegui4.mk:
	@echo CFLAGS += $(shell pkg-config --cflags libxfcegui4-1.0) >> $@
	@echo LIBS += $(shell pkg-config --libs libxfcegui4-1.0) >> $@

libwnck.mk:
	@echo CFLAGS += $(shell pkg-config --cflags libwnck-1.0) >> $@
	@echo LIBS += $(shell pkg-config --libs libwnck-1.0) >> $@

sinclude libxfce4panel.mk libxfcegui4.mk libwnck.mk

clean:
	@echo cleaning...
	@rm -f *.o *~

