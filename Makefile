main.gb: main.obj

%.obj: %.asm
	@echo "[1/3] Assembling file(s) $^ into $@..."
	rgbasm -E -o $@ $^

%.gb: %.obj
	@echo "\n[2/3] Linking file(s) $^ into ROM $@..."
	rgblink -o $@ -n $*.sym $^
	@echo "\n[3/3] Fixing header of the ROM $@..."
	rgbfix -v -p 0 $@
	@echo "\n[3/3] Done."

.PHONY: clean

clean:
	rm *.obj *.sym *.gb
