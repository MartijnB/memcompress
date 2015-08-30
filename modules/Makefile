MODULES  = $(shell find . -mindepth 1  -maxdepth 1 -type d | grep -v shared)

all:
	echo "Build modules..."

	make -C meminfo
	make -C memwrite
	make -C memread
	make -C memcompress
	make -C memchunkfinder

clean:
	echo "Clean modules..."

	make -C meminfo clean
	make -C memwrite clean
	make -C memread clean
	make -C memcompress clean
	make -C memchunkfinder clean