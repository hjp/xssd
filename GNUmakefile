-include GNUmakevars

# options for install to install a suid binary. 
# This can be overridden on the command line to allow building
# RPMs as unprivileged user.
SUIDPERM = -o root -m 4711 

config: GNUmakevars GNUmakerules

# Targets

all: xssd

rpm: xssd.rpm

xssd: xssd.o

install: \
    $(BUILD_ROOT)/$(BINDIR) \
    $(BUILD_ROOT)/$(BINDIR)/xssd \
    $(BUILD_ROOT)/$(MAN1DIR) \
    $(BUILD_ROOT)/$(MAN1DIR)/xssd.1 \
    $(BUILD_ROOT)/etc/xssd


clean:
	rm -f xssd.o xssd

distclean: clean
	rm -f xssd.d GNUmakevars


$(BUILD_ROOT)/$(BINDIR)/xssd: xssd
	$(INSTALL) $(SUIDPERM) $^ $@

$(BUILD_ROOT)/etc/xssd:
	$(INSTALL) -d $(BUILD_ROOT)/etc/xssd

%.tar.gz: %.spec
	tar zcf $@ $^

%: %.sh
	sh $^

xssd.tar.gz: \
    GNUmakefile  \
    GNUmakerules \
    GNUmakevars.sh \
    xssd.1 \
    xssd.c

%.rpm: %.tar.gz
	rpm -ta --clean --sign --rmsource $^ 

-include GNUmakerules
-include *.d
