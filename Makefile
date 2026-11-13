cc = clang
lflags = -lglfw -lvulkan
cflags = -O2
dflags = '' # by default not set

build/anyspace: src/main.c build/frag.spv build/vert.spv
	$(cc) src/main.c -o build/anyspace $(lflags) $(cflags) $(dflags)

build/vert.spv: src/vert.vert
	glslc src/vert.vert -o build/vert.spv

build/frag.spv: src/frag.frag
	glslc src/frag.frag -o build/frag.spv

.PHONY: test clean

test: build/anyspace
	cd build && ./anyspace

clean:
	rm -rf build
