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

MODE ?= release
CFLAGS := -std=c11 -municode -Wall -Wextra -Isrc
ifeq ($(MODE),debug)
  CFLAGS += -O0 -g -DDEBUG
else
  CFLAGS += -O2 -DNDEBUG
endif

LDFLAGS := -mwindows -municode -static -static-libgcc -s
LIBS    := -lopengl32 -lgdi32 -luser32 -lkernel32 -lcomdlg32 -lcomctl32 \
           -lshell32 -lole32 -ladvapi32 -ldwmapi -lwinmm

SRC := src/main.c src/cmdline.c src/util/log.c src/host_win32.c src/config_dialog.c
OBJ := $(patsubst src/%.c,build/obj/%.o,$(SRC))
RES := build/obj/screensaver.res

MAX_BYTES := 3145728

.PHONY: all release debug run config test clean

all: $(OUT)

release:
	@$(MAKE) -f $(THIS) MODE=release all

debug:
	@$(MAKE) -f $(THIS) MODE=debug all

build/obj/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

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

TEST_SRC   := build/tests/test_main.c build/tests/test_cmdline.c build/tests/test_log.c
TEST_UNITS := src/cmdline.c src/util/log.c

test:
	@mkdir -p build/obj
	$(CC) -std=c11 -Wall -Wextra -Isrc -Ibuild/tests \
	  $(TEST_SRC) $(TEST_UNITS) -o build/obj/run_tests.exe
	./build/obj/run_tests.exe

clean:
	rm -rf build/obj dist generated
