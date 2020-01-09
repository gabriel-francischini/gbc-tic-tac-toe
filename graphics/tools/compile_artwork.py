#/bin/python3
# PYTHON VERSION SHOULD BE BIGGER THAN 3.6
import os
import os.path
import sys
from PIL import Image
from functools import reduce


# This flag is setted whenever a function detects an ill-formed (but
# recoverable) input. The flag setting it should also print to the user what
# kind of ill-fomation led to flagging this variable.
compile_issue = False

pngpath = ''
x_start = 0
y_start = 0

def rgb2hex(r, g, b):
    def clamp(x):
        return max(0, min(x, 255))
    return "#{0:02x}{1:02x}{2:02x}".format(clamp(r), clamp(g), clamp(b))

def yield_image(pngpath):
    img = Image.open(pngpath)
    img = img.convert("RGB")
    return img

def yield_tiledata(pngpath, img_loader=yield_image):
    global x_start
    global y_start

    img = img_loader(pngpath)
    w, h = img.size

    is_divisible_by_8 = lambda x: (x % 8) == 0

    if not is_divisible_by_8(w) or not is_divisible_by_8(h):
        print(f'The file "{pngpath}" isn\'t made of 8x8 tiles, its size is '
              f'{w} ({w % 8} extra pixels) by {h} ({h % 8} extra pixels).')
        compile_issue = True

    for y_start in range(0, h, 8):
        for x_start in range(0, w, 8):
            tilecrop = img.crop([x_start, y_start, x_start + 8, y_start + 8])
            tiledata = tilecrop.load()

            x_start = x_start
            y_start = y_start
            yield tiledata, tilecrop

def yield_palettes(pngpath):
    for tiledata, tilecrop in yield_tiledata(pngpath):
        palette = set()

        for x in range(8):
            for y in range(8):
                palette.add(tiledata[x, y])

        # The GBC (CGB) supports at maximum a 3+1 palette
        if len(palette) > 4:
            print(f'The tile at position {x_start}w,{y_start}h of file '
                  f'{pngpath} has more than 4 colors.')
            compile_issue = True

        # Most PNGs use 8 bits per channel, but CGB uses only 5 bits per
        # channel. We have to be sure that the colors are in 5bpc.
        for color in palette:
            for channel in color:
                if channel % (2 ** (8 - 5)) != 0:
                    print(f'The color {rgb2hex(*color).upper()} '
                          f'[i.e. {str(color)}] at tile '
                          f'{x_start}w,{y_start}h of file {pngpath} '
                          f'isn\'t in the 0RRRRRGG GGGBBBBB format.')
                    compile_issue = True
        yield tuple(sorted(tuple(palette)))

artfolder_path = None

# Shows the correct usage in case user misused this script
if len(sys.argv) == 1:
    print(f'    USAGE: {sys.argv[0]} ARTFOLDER_PATH\n')
    exit()
else:
    artfolder_path = sys.argv[1]


print(f'Compiling PNG files in folder "{artfolder_path}" ...')

for (dirpath, dirnames, filenames) in os.walk(artfolder_path):
    pngpaths = [os.path.join(artfolder_path, f)
                for f in filenames
                if f.lower().endswith('.png')]
    break


palettes = set()

# We should do a first pass analyzing the images in order to see if the images
# match with the Game Boy Color's display limitations (i.e. 8x8 tiles with 3+1
# palettes). Those checks are cortesy of yield_palettes.
# Aftewards, we join every file palettes into a big list of palettes (excluding
# duplicates).
palettes = set(reduce(lambda x, y: list(x) + list(y),
                      map(yield_palettes, pngpaths)))


if compile_issue is True:
    print("The compilation of the art assets can't proceed if the above "
          "issues aren't fixed.")
    exit()

# Sometimes a tile doesn't uses all colors of it's palette, yet it still uses
# that particular palette. In other words, the tile's palette is a subset of
# another "general" palette.
for palette_a in list(palettes):
    for palette_b in list(palettes):
        if (set(palette_a) < set(palette_b)) and palette_a in palettes:
            palettes.remove(palette_a)


palettes = list(palettes)
palettes.sort(key=len)
for palette in palettes:
    print(sorted(list(map(lambda x: rgb2hex(*x), palette))))
