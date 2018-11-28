.PHONY: disassembler

CC = gcc

XCODE_PROJ := Space\ Invaders
OSX_EXECUTABLE := ./$(XCODE_PROJ)/Build/Products/Debug/$(XCODE_PROJ).app/Contents/MacOS/$(XCODE_PROJ)
OSX_RUN_CMD := $(OSX_EXECUTABLE)

CFLAGS := -ggdb -g --std=c99 # -Wall -Wextra
ifdef DEBUG
    CFLAGS += -D DEBUG
    OSX_RUN_CMD := lldb $(OSX_EXECUTABLE)
endif


EMULATOR_OBJS = emulator.c disassembler.c
DISASSEMBLER_OBJS = $(EMULATOR_OBJS) tests/disassembler.c
TEST_OBJS = $(EMULATOR_OBJS) tests/emulator.c
LINUX_OBJS = $(EMULATOR_OBJS) linux.c

emulator: $(EMULATOR_OBJS)
	$(CC) $(CFLAGS) $(EMULATOR_OBJS) -o emulator
	./emulator invaders.h invaders.g invaders.f invaders.e

disassembler: $(DISASSEMBLER_OBJS)
	$(CC) $(DISASSEMBLER_OBJS) -o disassembler
	./disassembler invaders.h invaders.g invaders.f invaders.e

clean:
	rm -f *.o emulator

test: $(TEST_OBJS)
	$(CC) -D DEBUG -D CPU_TEST $(CFLAGS) $(EMULATOR_OBJS) tests/emulator.c -o emulator
	./emulator --start 0x100 cpudiag.bin | head -n 612 | grep -q 'CPU IS OPERATIONAL'

$(OSX_EXECUTABLE):
	xcodebuild -scheme $(XCODE_PROJ) -project $(XCODE_PROJ)/$(XCODE_PROJ).xcodeproj build

osx: $(OSX_EXECUTABLE)
	$(OSX_RUN_CMD)

# wasm: $(EMULATOR_OBJS)
# 	# Must source ./emsdk_env.sh and then run this target until Bazel is implemented
# 	emcc disassembler.c emulator.c wasm.c -o index.html -s USE_PTHREADS=1 -s WASM=0 -s USE_SDL=2 --embed-file invaders.h --embed-file invaders.g --embed-file invaders.f --embed-file invaders.e

# TODO: Replace include and library paths when bazel build is implemented
linux: $(LINUX_OBJS)
	$(CC) $(CFLAGS) -I ../SDL-mirror/include -L ../SDL-mirror/build/build/.libs disassembler.c emulator.c linux.c -l SDL2 -lpthread -o linux
