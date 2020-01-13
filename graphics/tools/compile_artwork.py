#/bin/python3
# PYTHON VERSION SHOULD BE BIGGER THAN 3.6
import os
import os.path
import sys
from PIL import Image
from PIL import ImageOps
from functools import reduce
import itertools
from tqdm import tqdm


# This flag is setted whenever a function detects an ill-formed (but
# recoverable) input. The flag setting it should also print to the user what
# kind of ill-fomation led to flagging this variable.
compile_issue = False

pngpath = ''
x_start = 0
y_start = 0
script_name = sys.argv[0].rsplit('.', 1)[0]
alpha_color = (240, 240, 232)

def rgb2hex(r, g, b):
    def clamp(x):
        return max(0, min(x, 255))
    return "#{0:02x}{1:02x}{2:02x}".format(clamp(r), clamp(g), clamp(b))

def sort_palette(palette):
    return tuple(sorted(tuple(map(tuple, palette)),
                        key=lambda x: -1 if x == alpha_color else sum(x)))

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

def extract_palette(tiledata):
        palette = set()

        for x in range(8):
            for y in range(8):
                palette.add(tiledata[x, y])

        return sort_palette(tuple(palette))

def yield_palettes(pngpath):
    for tiledata, tilecrop in yield_tiledata(pngpath):
        palette = extract_palette(tiledata)

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
        yield sort_palette(palette)

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
    pngpaths.sort()
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


palettes = list(map(sort_palette, palettes))
palettes.sort(key=len)


# We know should gather some data about palette usage in order to build the
# final game's palette bank(s). We also will use this usage data to warn the
# user about potential palette clashes or palette misuse.
histogram = {}
for pngpath in pngpaths:
    for palette in yield_palettes(pngpath):
        palette = set(palette)

        for model_palette in palettes[::-1]:

            # Pads the palette so we ALWAYS have 4 colors
            while len(model_palette) < 4:
                model_palette += (alpha_color, )

            if palette <= set(model_palette):
                histogram[model_palette] = (histogram.get(model_palette, [])
                                            + [(pngpath, x_start, y_start)])
                break


histogram = sorted(histogram.items(), key=lambda x: len(x[-1]), reverse=True)
final_palettes = [pal for pal, occurs in histogram]


# Outputs a palette overview for the user AND a detailed description of the
# palettes.
with open(script_name + '.log', 'w') as logfile:
    print('The following palettes were found:\n')
    for index, (palette, occurs) in enumerate(histogram):
        colors_str = ", ".join(map(lambda x: rgb2hex(*x), palette))
        header_str = f'#{index}: {colors_str:<36} -- {len(occurs):>5} usages'
        print(' ' * 4 + header_str)
        logfile.write(header_str + '\n')

        for (pngpath, x_start, y_start) in occurs:
            logfile.write(' ' * 4
                          + f'tile at {x_start:>3}w, {y_start:>3}h '
                          f'of file {pngpath}\n')

        logfile.write('\n\n\n')

print('')


# With all the usage data we finally can output a nice PNG art representing the
# game's palette banks.
palette_histogram = dict([(k, len(v)) for k, v in histogram])
del histogram

palette_image = Image.new('RGB', (4, len(final_palettes)))
palette_image_data = palette_image.load()
for h in range(len(final_palettes)):
    for w in range(4):
        palette_image_data[w, h] = final_palettes[h][w]

palette_image.save('palettes_overview_log.png')


#
# With all the palette analysis done, we finally can analyze the tiles.
#

def to_final_palette(palette):
    for final_palette in final_palettes:
        if set(palette) <= set(final_palette):
            return final_palette

def indexed_tiledata(tiledata, palette):
    acc = []

    for y in range(8):
        for x in range(8):
            acc += [palette.index(tiledata[x, y])]
    return acc

vflip = lambda x: ImageOps.flip(x)
hflip = lambda x: ImageOps.mirror(x)
dflip = lambda x: vflip(hflip(x))
noflip = lambda x: x


tiles = {}
tiles_order = {}
usual_tile_palette = {}

def to_final_tile(tilecrop, final_palette):
    for indexed_final_tile in sorted(tiles.keys(),
                                     key=lambda x: tiles[x],
                                     reverse=True):
        for mapper in [noflip, hflip, vflip, dflip]:
            indexed_tile = tuple(indexed_tiledata(mapper(tilecrop).load(), final_palette))
            if indexed_final_tile == indexed_tile:
                return indexed_final_tile, mapper

    return None, None


print("Compressing tiles into tilemap, please wait...\n")
tiles_seen = 0
total = len([1 for x, y in itertools.chain(*map(yield_tiledata, pngpaths))])
for tiledata, tilecrop in tqdm(itertools.chain(*map(yield_tiledata, pngpaths)),
                               total=total):
    palette = extract_palette(tiledata)
    palette = to_final_palette(palette)

    indexed_final_tile, fliptype = to_final_tile(tilecrop, palette)

    if indexed_final_tile is not None:
        tiles[indexed_final_tile] += 1
    else:
        indexed_tile = tuple(indexed_tiledata(tiledata, palette))
        tiles[indexed_tile] = tiles.get(indexed_tile, 0) + 1
        usual_tile_palette[indexed_tile] = palette
        tiles_order[indexed_tile] = tiles_seen
        tiles_seen += 1

print("")
print(f"We found {len(tiles.keys())} tiles, used across {sum(tiles.values())}"
      " occurrences.")


def convert_tile_to_ascii(tile):
    k = tile
    k = list(k)
    m2 = {0: ' ', 3: '▓', 2: '▒', 1:'░'}
    m = {0: ' ', 3: 'M', 2: ':', 1:'.'}
    tilestr = ""
    for l in range(0, 64, 8):
         tilestr +=('\n'
                    + "".join(map(lambda x: m[x], k[l:l+8]))
                    + "."
                    + "".join(map(lambda x: m2[x], k[l:l+8])))
    return tilestr


def output_tiles(tilelist, pngpath):
    tiles = tilelist
    side = int(len(tiles)**0.5)+1
    tilemap_image = Image.new('RGB', (side * 8, side * 8))
    tilemap_data = tilemap_image.load()

    x = 0
    y = 0
    for tile in tiles:
        palette = usual_tile_palette[tile]
        color_guide = dict(enumerate(palette))

        tile = [color_guide[x] for x in tile]

        for h in range(8):
            for w in range(8):
                tilemap_data[x + w, y + h] = tile[h * 8 + w]

        x += 8
        if x >= (side * 8):
            x -= (side * 8)
            y += 8

    tilemap_image.save(pngpath)

output_tiles(sorted(tiles.keys(), key=lambda x: tiles[x], reverse=True),
             'tilemap_usage_order_log.png')

output_tiles(sorted(tiles.keys(), key=lambda x: tiles_order[x]),
             'tilemap_overview_log.png')

asmfile = open(os.path.join(artfolder_path, 'compiled_artwork.asm'), 'w')

def as_safe_str(part):
    return ''.join([x if x.isalnum() else '_' for x in part]).strip()

for part in artfolder_path.split('/')[::-1]:
    part = as_safe_str(part)
    length = len(''.join([x for x in part if x.isalnum()]).strip())

    if length > 0:
        prefix = part.strip()
        break


asmprint = lambda x: asmfile.write(x)

asmprint(f'''; THIS FILE WAS AUTOGENERATED BY {script_name}
; It should contain the graphic's data for folder {artfolder_path}
; The unique prefix for these graphics is "{prefix}"


section "__compiled_artwork__{prefix}__", ROM0


CG_PaletteBank__{prefix}:
''')

indent = '''                '''

fmt = '''

;{indent}This palette #{number} occured in {occurs} tiles
{palettelabel}:
'''

def palette_label(final_palette):
    global prefix
    number = final_palettes.index(final_palette)
    return f'CG_Palette__{prefix}__{number}'

fmtcolor = ''';{indent}HEX: {hexcolor} RGB: {r:>3} {g:>3} {b:>3}
;{indent}    xBbBbBGgGgGRrRrR
{indent} dw %0{bbin}{gbin}{rbin}

'''
for number, palette in enumerate(final_palettes):
    palettelabel = palette_label(palette)
    occurs = palette_histogram[palette]
    asmprint(fmt.format(**locals()))

    for r, g, b in palette:
        hexcolor = rgb2hex(r, g, b).upper()
        gbc_bin = lambda x: "{0:08b}".format(x)[:-3]
        rbin = gbc_bin(r)
        gbin = gbc_bin(g)
        bbin = gbc_bin(b)

        asmprint(fmtcolor.format(**locals()))


def tile_label(indexed_tile):
    global prefix
    number = tiles_order[indexed_tile]
    return f'CG_TILE__{prefix}__{number}'


fmt = '''


;{indent} TILE #{number} -- used {usage} times
;
{tileascii}
{tilelabel}:
;{indent}    abcdefgh,  ABCDEFGH
'''
fmtline = '''{indent} db %{lowerbyte}, %{higherbyte}
'''
for number, tile in enumerate(sorted(tiles.keys(),
                                     key=lambda x: tiles_order[x])):
    number = tiles_order[tile]
    tilelabel = tile_label(tile)
    usage = tiles[tile]
    tileascii = (convert_tile_to_ascii(tile)
                 .replace('\n',f'.\n;{indent}.')
                 .lstrip()
                 .lstrip(f'.\n{indent}') + '.')
    asmprint(fmt.format(**locals()))

    hb = {0: '0', 1: '0', 2: '1', 3: '1'}
    lb = {0: '0', 1: '1', 2: '0', 3: '1'}

    for l in range(0, 64, 8):
        line = tile[l:l+8]
        higherbyte = ''.join(map(lambda x: hb[x], line))
        lowerbyte = ''.join(map(lambda x: lb[x], line))
        asmprint(fmtline.format(**locals()))


fmt = '''



;{indent}Sprite of file {spritepath}
;{indent}Size: {w}px x {h}px, or {tw} tiles x {th} tiles
CG_SPRITE__{prefix}__{sprite_prefix}:
.length:{length_indent} db {length}
.size:
.size_x:{size_indent} db {tw}
.size_y:{size_indent} db {th}
.tiledata:
'''

fmt_tile = '''.tile_x{xt_start:02}_y{yt_start:02}:{tileindent} dw {tilelabel}
'''

fmt_metadata_header = '''; Metadata for this tile, PPP=Palette n., B=bank, p=priority
;{indent}   pVH0BPPP
.metadata:
'''

fmt_tile_metadata = ('''.meta_x{xt_start:02}_y{yt_start:02}:'''
                     '''{metaindent} db 0{flipmodebin}00{palettebin} ; {tile_desc:<23} on {tilelabel}
''')

str_tile = ''
str_metadata = ''

print("\nCompiling sprite data, please wait...\n")
for spritepath in tqdm(pngpaths):
    sprite_prefix = as_safe_str(os.path.splitext(os.path.basename(spritepath))[0])
    img = yield_image(spritepath)
    w, h = img.size
    tw, th = w // 8, h // 8
    length = tw * th
    size_indent = ' ' * (len(indent) - len('.size_x:'))
    tileindent = ' ' * (len(indent) - len('.tile_x00_y00:'))
    metaindent = ' ' * (len(indent) - len('.meta_x00_y00:'))
    length_indent = ' ' * (len(indent) - len('.length:'))

    asmprint(fmt.format(**locals()))

    for tiledata, tilecrop in yield_tiledata(spritepath):
        palette = to_final_palette(extract_palette(tiledata))
        final_tile, flipmode = to_final_tile(tilecrop, palette)

        palettelabel = palette_label(palette)
        tilelabel = tile_label(final_tile)

        flipmapper = {noflip: '00', hflip: '01', vflip: '10', dflip: '11'}
        flipstr = {noflip: 'no mirror', hflip: 'Mirror Horiz.',
                   vflip: 'Mirror Vert.', dflip: 'Mirror H. & V.'}

        flipmodebin = flipmapper[flipmode]
        palette_num = final_palettes.index(palette)
        palettebin = '{:03b}'.format(palette_num)

        tile_desc = 'PAL #{}, {}'.format(palette_num, flipstr[flipmode])
        xt_start = x_start // 8
        yt_start = y_start // 8

        str_tile += fmt_tile.format(**locals())
        str_metadata += fmt_tile_metadata.format(**locals())


    asmprint(str_tile)
    asmprint(fmt_metadata_header.format(**locals()))
    asmprint(str_metadata)

    str_tile = ''
    str_metadata = ''

asmfile.close()
