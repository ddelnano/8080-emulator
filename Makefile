.PHONY: disassembler

CC = gcc

XCODE_PROJ := Space\ Invaders
OSX_EXECUTABLE := ./$(XCODE_PROJ)/Build/Products/Debug/$(XCODE_PROJ).app/Contents/MacOS/$(XCODE_PROJ)
OSX_RUN_CMD := $(OSX_EXECUTABLE)

ifdef DEBUG
    CFLAGS = -D DEBUG -ggdb
    OSX_RUN_CMD := lldb $(OSX_EXECUTABLE)
else
    CFLAGS = -ggdb -g
endif


EMULATOR_OBJS = emulator.c disassembler.c wasm.c
DISASSEMBLER_OBJS = disassembler.c
TEST_OBJS = tests/emulator.c tests/disassembler.c

emulator: $(EMULATOR_OBJS)
	$(CC) $(CFLAGS) $(EMULATOR_OBJS) -o emulator
	./emulator invaders.h invaders.g invaders.f invaders.e

disassembler: $(DISASSEMBLER_OBJS)
	$(CC) $(DISASSEMBLER_OBJS) -o disassembler
	./disassembler invaders.h invaders.g invaders.f invaders.e

clean:
	rm -f *.o emulator

test: $(EMULATOR_OBJS) $(TEST_OBJS)
	$(CC) -D DEBUG -D CPU_TEST $(CFLAGS) $(EMULATOR_OBJS) -o emulator
	./emulator --start 0x100 cpudiag.bin | head -n 612 | grep 'CPU IS OPERATIONAL'

$(OSX_EXECUTABLE):
	xcodebuild -scheme $(XCODE_PROJ) -project $(XCODE_PROJ)/$(XCODE_PROJ).xcodeproj build

osx: $(OSX_EXECUTABLE)
	$(OSX_RUN_CMD)

# TODO: Need a proper build for emcc to be available
wasm: $(EMULATOR_OBJS)
	emcc disassembler.c emulator.c wasm.c -o index.html -s USE_PTHREADS=1 -s WASM=0 -s USE_SDL=2 --embed-file invaders.h --embed-file invaders.g --embed-file invaders.f --embed-file invaders.e

linux: $(EMULATOR_OBJS)
	$(CC) $(CFLAGS) -I ../SDL-mirror/include -L ../SDL-mirror/build/build/.libs disassembler.c emulator.c wasm.c -l SDL2 -lpthread -o linux
