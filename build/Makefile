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
CXX     := g++
WINDRES := windres
NAME    := Modern3DText
OUT     := dist/$(NAME).scr

INCLUDES := -Isrc -Ithird_party -Ithird_party/glad/include \
            -Ithird_party/libtess2/Include -Ithird_party/clipper2/Include -Igenerated

MODE ?= release
CFLAGS := -std=c11 -municode -Wall -Wextra $(INCLUDES)
CXXFLAGS := -std=c++17 -municode -Wall -Wextra $(INCLUDES)
ifeq ($(MODE),debug)
  CFLAGS   += -O0 -g -DDEBUG
  CXXFLAGS += -O0 -g -DDEBUG
else
  CFLAGS   += -O2 -DNDEBUG
  CXXFLAGS += -O2 -DNDEBUG
endif

# link final com g++ (nao gcc): so ele sabe linkar libstdc++ corretamente
# (inclusive -static-libstdc++) - a unidade C++ e' so' robust_offset.cpp +
# Clipper2 (offset de poligono robusto pro bevel, ver contour_mesh.c); todo
# o resto do projeto continua C puro, compilado por $(CC) como sempre.
LDFLAGS := -mwindows -municode -static -static-libgcc -static-libstdc++ -s
LIBS    := -lopengl32 -lgdi32 -luser32 -lkernel32 -lcomdlg32 -lcomctl32 \
           -lshell32 -lole32 -ladvapi32 -ldwmapi -lwinmm -lversion

SRC_C := src/main.c src/cmdline.c src/util/log.c src/util/mathx.c src/util/clockfmt.c src/gl_core.c \
         src/gl_window.c src/host_win32.c src/config_dialog.c src/config.c src/presets.c \
         src/material.c src/scene.c src/env.c src/post.c src/particles.c \
         src/render_tiers.c src/render_tiers_gl.c src/i18n.c \
         src/geometry/font_outline.c src/geometry/stb_impl.c src/geometry/contour_mesh.c \
         src/geometry/svg_shapes.c src/geometry/mesh_import.c src/geometry/feature_edges.c
SRC_CPP := src/geometry/robust_offset.cpp
TP_C  := third_party/glad/src/gl.c $(wildcard third_party/libtess2/Source/*.c)
TP_CPP := third_party/clipper2/Source/clipper.engine.cpp third_party/clipper2/Source/clipper.offset.cpp

SRC_OBJ := $(patsubst src/%.c,build/obj/src/%.o,$(SRC_C)) $(patsubst src/%.cpp,build/obj/src/%.o,$(SRC_CPP))
TP_OBJ  := $(patsubst third_party/%.c,build/obj/tp/%.o,$(TP_C)) $(patsubst third_party/%.cpp,build/obj/tp/%.o,$(TP_CPP))
OBJ     := $(SRC_OBJ) $(TP_OBJ)
RES     := build/obj/screensaver.res

EMBED_INPUTS := shaders/model.vert shaders/model.frag shaders/fullscreen.vert \
                shaders/post_bright.frag shaders/post_down.frag shaders/post_up.frag \
                shaders/post_combine.frag shaders/post_streak.frag \
                shaders/post_finish.frag shaders/post_fxaa.frag shaders/background.frag \
                shaders/particle.vert shaders/particle.frag shaders/wboit_resolve.frag \
                shaders/wireframe.vert shaders/wireframe.geom shaders/wireframe.frag \
                res/lang/pt.txt res/lang/en.txt res/env_default.jpg
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

build/obj/src/%.o: src/%.cpp $(EMBEDDED_H)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# stb single-header implementation: nao aplicar -Wall/-Wextra (codigo de terceiros)
build/obj/src/geometry/stb_impl.o: src/geometry/stb_impl.c
	@mkdir -p $(dir $@)
	$(CC) $(filter-out -Wall -Wextra,$(CFLAGS)) -w -c $< -o $@

build/obj/tp/%.o: third_party/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -w -c $< -o $@

build/obj/tp/%.o: third_party/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -w -c $< -o $@

$(RES): res/screensaver.rc res/screensaver.manifest
	@mkdir -p build/obj
	$(WINDRES) --include-dir res --include-dir src -O coff $< -o $@

$(OUT): $(OBJ) $(RES)
	@mkdir -p dist
	$(CXX) $(OBJ) $(RES) -o $@ $(LDFLAGS) $(LIBS)
	@sz=`wc -c < $@`; echo "Built $@ ($$sz bytes)"; \
	 if [ $$sz -gt $(MAX_BYTES) ]; then echo "ERRO: passou de 3 MB"; exit 1; fi

run: debug
	./$(OUT) /s

config: debug
	./$(OUT) /c

TEST_SRC   := build/tests/test_main.c build/tests/test_cmdline.c build/tests/test_log.c \
              build/tests/test_mathx.c build/tests/test_font_outline.c \
              build/tests/test_feature_edges.c \
              build/tests/test_contour_mesh.c build/tests/test_config.c \
              build/tests/test_render_tiers.c build/tests/test_clockfmt.c build/tests/test_svg_shapes.c \
              build/tests/test_mesh_import.c build/tests/test_i18n.c build/tests/test_presets.c
TEST_UNITS := src/cmdline.c src/util/log.c src/util/mathx.c src/util/clockfmt.c \
              src/geometry/font_outline.c src/geometry/feature_edges.c src/geometry/contour_mesh.c src/config.c \
              src/render_tiers.c src/geometry/svg_shapes.c src/geometry/mesh_import.c \
              src/i18n.c src/presets.c
TEST_INC   := -Isrc -Ithird_party -Ithird_party/libtess2/Include -Ithird_party/clipper2/Include \
              -Ibuild/tests -Igenerated
LIBTESS_C  := $(wildcard third_party/libtess2/Source/*.c)
CLIPPER_CPP := third_party/clipper2/Source/clipper.engine.cpp third_party/clipper2/Source/clipper.offset.cpp

LIBTESS_TEST_OBJ  := $(patsubst third_party/libtess2/Source/%.c,build/obj/test/libtess_%.o,$(LIBTESS_C))
CLIPPER_TEST_OBJ  := $(patsubst third_party/clipper2/Source/%.cpp,build/obj/test/clipper_%.o,$(CLIPPER_CPP))

build/obj/test/libtess_%.o: third_party/libtess2/Source/%.c
	@mkdir -p build/obj/test
	$(CC) -std=c11 -w $(TEST_INC) -c $< -o $@

build/obj/test/clipper_%.o: third_party/clipper2/Source/%.cpp
	@mkdir -p build/obj/test
	$(CXX) -std=c++17 -w $(TEST_INC) -c $< -o $@

build/obj/test/robust_offset.o: src/geometry/robust_offset.cpp
	@mkdir -p build/obj/test
	$(CXX) -std=c++17 -Wall -Wextra $(TEST_INC) -c $< -o $@

build/obj/test/stb_impl.o: src/geometry/stb_impl.c
	@mkdir -p build/obj/test
	$(CC) -std=c11 -w -Isrc -Ithird_party -c $< -o $@

# link com $(CC) (gcc, nao g++): os fontes .c do teste sao compilados e
# linkados numa unica chamada aqui, e g++ trataria esses .c como C++ - so'
# a robust_offset.o/clipper_*.o (ja compiladas em C++ acima) precisam de
# libstdc++, adicionada manualmente via -lstdc++ (link dinamico aqui basta,
# run_tests.exe e' so' uma ferramenta de dev, nao o artefato distribuido).
test: build/obj/test/stb_impl.o $(LIBTESS_TEST_OBJ) $(CLIPPER_TEST_OBJ) build/obj/test/robust_offset.o $(EMBEDDED_H)
	$(CC) -std=c11 -Wall -Wextra $(TEST_INC) \
	  $(TEST_SRC) $(TEST_UNITS) $^ \
	  -o build/obj/run_tests.exe -lm -lgdi32 -luser32 -ladvapi32 -lkernel32 -lstdc++
	./build/obj/run_tests.exe

clean:
	rm -rf build/obj dist generated
