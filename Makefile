all: clean build copy-assets

build:
	mkdir bin
	gcc -Wall -pthread -o bin/jecho src/main.c

copy-assets:
	cp -r example bin/

run:
	bin/jecho

clean:
	rm -rf bin