include GNUmakevars

all: xssd

xssd: xssd.o

install: $(BINDIR)/xssd $(MAN1DIR)/xssd.1

clean:
	rm -f xssd.o xssd

distclean: clean
	rm -f xssd.d

$(BINDIR)/xssd: xssd
	$(INSTALL) -o root -m 4711 $^ $@


include GNUmakerules
-include *.d
