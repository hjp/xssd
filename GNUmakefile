include GNUmakevars

all: xssd

xssd:

install: $(BINDIR)/xssd $(MAN1DIR)/xssd.1

$(BINDIR)/xssd: xssd
	$(INSTALL) -o root -m 4711 $^ $@


include GNUmakerules
-include *.d
