# Modern 3D Text - build (w64devkit)
#
# Rode a partir da RAIZ do projeto (D:/Downloads/Screensaver), num shell com o
# w64devkit no PATH (o w64devkit.exe abre um; ou adicione <w64devkit>\bin ao PATH):
#
#   mingw32-make -f build/Makefile           # release  -> dist/Modern3DText.scr
#   mingw32-make -f build/Makefile debug
#   mingw32-make -f build/Makefile run        # builda debug e roda /s
#   mingw32-make -f build/Makefile config     # builda debug e roda /c
#   mingw32-make -f build/Makefile test       # testes unitarios
#   mingw32-make -f build/Makefile clean

THIS := $(lastword $(MAKEFILE_LIST))

CC      := gcc
WINDRES := windres
NAME    := Modern3DText
OUT     := dist/$(NAME).scr

INCLUDES := -Isrc -Ithird_party -Ithird_party/glad/include \
            -Ithird_party/libtess2/Include -Igenerated

MODE ?= release
CFLAGS := -std=c11 -municode -Wall -Wextra $(INCLUDES)
ifeq ($(MODE),debug)
  CFLAGS += -O0 -g -DDEBUG
else
  CFLAGS += -O2 -DNDEBUG
endif

LDFLAGS := -mwindows -municode -static -static-libgcc -s
LIBS    := -lopengl32 -lgdi32 -luser32 -lkernel32 -lcomdlg32 -lcomctl32 \
           -lshell32 -lole32 -ladvapi32 -ldwmapi -lwinmm

SRC_C := src/main.c src/cmdline.c src/util/log.c src/util/mathx.c src/gl_core.c \
         src/gl_window.c src/host_win32.c src/config_dialog.c src/config.c \
         src/material.c src/scene.c src/env.c src/post.c \
         src/geometry/font_outline.c src/geometry/stb_impl.c src/geometry/contour_mesh.c \
         src/geometry/sdf.c
TP_C  := third_party/glad/src/gl.c $(wildcard third_party/libtess2/Source/*.c)

SRC_OBJ := $(patsubst src/%.c,build/obj/src/%.o,$(SRC_C))
TP_OBJ  := $(patsubst third_party/%.c,build/obj/tp/%.o,$(TP_C))
OBJ     := $(SRC_OBJ) $(TP_OBJ)
RES     := build/obj/screensaver.res

EMBED_INPUTS := shaders/model.vert shaders/model.frag shaders/fullscreen.vert \
                shaders/post_bright.frag shaders/post_down.frag shaders/post_up.frag \
                shaders/post_composite.frag
EMBEDDED_H   := generated/embedded.h

MAX_BYTES := 3145728

.PHONY: all release debug run config test clean

all: $(OUT)

release:
	@$(MAKE) -f $(THIS) MODE=release all

debug:
	@$(MAKE) -f $(THIS) MODE=debug all

$(EMBEDDED_H): build/tools/embed.c $(EMBED_INPUTS)
	@mkdir -p generated build/obj
	$(CC) -O2 -std=c11 build/tools/embed.c -o build/obj/embed.exe
	./build/obj/embed.exe $(EMBEDDED_H) $(EMBED_INPUTS)

build/obj/src/%.o: src/%.c $(EMBEDDED_H)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# stb single-header implementation: nao aplicar -Wall/-Wextra (codigo de terceiros)
build/obj/src/geometry/stb_impl.o: src/geometry/stb_impl.c
	@mkdir -p $(dir $@)
	$(CC) $(filter-out -Wall -Wextra,$(CFLAGS)) -w -c $< -o $@

build/obj/tp/%.o: third_party/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -w -c $< -o $@

$(RES): res/screensaver.rc res/screensaver.manifest
	@mkdir -p build/obj
	$(WINDRES) --include-dir res --include-dir src -O coff $< -o $@

$(OUT): $(OBJ) $(RES)
	@mkdir -p dist
	$(CC) $(OBJ) $(RES) -o $@ $(LDFLAGS) $(LIBS)
	@sz=`wc -c < $@`; echo "Built $@ ($$sz bytes)"; \
	 if [ $$sz -gt $(MAX_BYTES) ]; then echo "ERRO: passou de 3 MB"; exit 1; fi

run: debug
	./$(OUT) /s

config: debug
	./$(OUT) /c

TEST_SRC   := build/tests/test_main.c build/tests/test_cmdline.c build/tests/test_log.c \
              build/tests/test_mathx.c build/tests/test_font_outline.c \
              build/tests/test_contour_mesh.c build/tests/test_config.c build/tests/test_sdf.c
TEST_UNITS := src/cmdline.c src/util/log.c src/util/mathx.c \
              src/geometry/font_outline.c src/geometry/contour_mesh.c src/config.c \
              src/geometry/sdf.c
TEST_INC   := -Isrc -Ithird_party -Ithird_party/libtess2/Include -Ibuild/tests
LIBTESS_C  := $(wildcard third_party/libtess2/Source/*.c)

LIBTESS_TEST_OBJ := $(patsubst third_party/libtess2/Source/%.c,build/obj/test/libtess_%.o,$(LIBTESS_C))

build/obj/test/libtess_%.o: third_party/libtess2/Source/%.c
	@mkdir -p build/obj/test
	$(CC) -std=c11 -w $(TEST_INC) -c $< -o $@

build/obj/test/stb_impl.o: src/geometry/stb_impl.c
	@mkdir -p build/obj/test
	$(CC) -std=c11 -w -Isrc -Ithird_party -c $< -o $@

test: build/obj/test/stb_impl.o $(LIBTESS_TEST_OBJ)
	$(CC) -std=c11 -Wall -Wextra $(TEST_INC) \
	  $(TEST_SRC) $(TEST_UNITS) $^ \
	  -o build/obj/run_tests.exe -lm -lgdi32 -luser32 -ladvapi32
	./build/obj/run_tests.exe

clean:
	rm -rf build/obj dist generated
