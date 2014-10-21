.PHONY: build clean dist
VERSION := 0.0.3

TAR := loggen-$(VERSION)-linux-amd64.tar.gz

build : loggen

dist : build $(TAR)

loggen : loggen.cpp
	(g++ -o loggen *.cpp)

clean :
	rm -f loggen

deb :
	@echo "Building loggen .deb..."
	$(MAKE) fpm
	mkdir -p build/usr/local/bin/
	cp loggen build/usr/local/bin
	fpm -C build \
			-s dir -t deb \
			-v $(VERSION) \
			-a all \
			-n loggen \
			./usr/local/bin/loggen

$(TAR):
	@echo "Building loggen .tar.gz..."
	tar -czf $@ loggen

fpm :
	@echo "Checking for fpm gem..."
	@(gem list | grep ^fpm > /dev/null || gem install fpm)
