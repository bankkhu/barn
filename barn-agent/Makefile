# Please make sure CPLUS_INCLUDE_PATH includes boost's include path and
# LIBRARY_PATH includes boost's library path.

.PHONY: build clean dist canary fpm test

VERSION := 0.6.65
DEB := barn-agent_$(VERSION)_all.deb
TAR := barn-agent-$(VERSION)-linux-amd64.tar.gz
CANARY := barn-agent-canary-$(VERSION)-linux-amd64.tar.gz

default : test

build :
	scons

test : build
	./test/barn_test

dist : test $(DEB) $(TAR)

canary : test $(CANARY)

clean :
	scons -c
	rm -rf build
	rm -f *.deb

$(DEB) :
	@echo "Building agent .deb..."
	mkdir -p build/usr/local/bin/
	cp barn-agent build/usr/local/bin
	$(MAKE) fpm
	fpm -C build \
			-s dir -t deb \
			-v $(VERSION) \
			-a all \
			-n barn-agent \
			-d bash \
			-d rsync \
			-d inotify-tools \
			-d ganglia-monitor \
			./usr/local/bin/barn-agent

$(TAR):
	@echo "Building agent .tar.gz..."
	tar -czf $@ barn-agent

$(CANARY) :
	@echo "Building agent canary .tar.gz..."
	mkdir -p build/usr/local/bin/
	cp barn-agent barn-agent-canary
	tar -czf $@ barn-agent-canary

fpm :
	@echo "Checking for fpm gem..."
	@(gem list | grep ^fpm > /dev/null || gem install fpm)
