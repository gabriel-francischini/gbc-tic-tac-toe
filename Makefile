OBJ=main.obj

ASM=initialization.asm \
	graphics/graphics.asm \
	utility.asm \
	graphics/artwork/compiled_artwork.asm


main.gb: $(OBJ) hardware.inc $(ASM)
	@echo "\n--> Linking file(s) $(OBJ) into ROM main.gb..."
	rgblink -o main.gb -n main.sym $(OBJ)
	@echo "\n--> Fixing header of the ROM main.gb..."
	rgbfix -v -p 0 main.gb
	@echo "\n--> Done."

main.obj: hardware.inc $(ASM) debugger_wrapper
	@echo "--> Assembling file(s) $^ into $@..."
	rgbasm -E -o main.obj main.asm

%.obj: %.asm
	@echo "--> Assembling file(s) $^ into $@..."
	rgbasm -E -o $@ $^

debugger_wrapper: debugger_wrapper.cpp
	@echo "\n--> Compiling the debugger wrapper utility from $^..."
	g++ -std=c++1z -g -v -o $@ $^ -lutil

.PHONY: clean clear

clean:
	@rm -fv *.obj $(OBJ)

clear: clean
	@rm -fv *.sym *.gb debugger_wrapper
