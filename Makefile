OBJ=main.obj initialization.obj graphics.obj utility.obj

main.gb: $(OBJ) hardware.inc
	@echo "\n--> Linking file(s) $(OBJ) into ROM main.gb..."
	rgblink -o main.gb -n main.sym $(OBJ)
	@echo "\n--> Fixing header of the ROM main.gb..."
	rgbfix -v -p 0 main.gb
	@echo "\n--> Done."

%.obj: %.asm
	@echo "--> Assembling file(s) $^ into $@..."
	rgbasm -E -o $@ $^


.PHONY: clean clear

clean:
	rm *.obj

clear: clean
	rm *.sym *.gb
