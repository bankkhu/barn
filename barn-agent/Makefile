# Please make sure CPLUS_INCLUDE_PATH includes boost's include path and
# LIBRARY_PATH includes boost's library path.

.PHONY: build clean dist fpm test

VERSION := 0.5.13
DEB := barn-agent_$(VERSION)_all.deb

default : test

build :
	scons

test : build
	./test/barn_test

dist : build $(DEB)

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

fpm :
	@echo "Checking for fpm gem..."
	@(gem list | grep ^fpm > /dev/null || gem install fpm)
