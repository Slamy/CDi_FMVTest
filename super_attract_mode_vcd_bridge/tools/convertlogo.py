# partially generated using ChatGPT

from PIL import Image

def gif_to_c_arrays(gif_path):
    img = Image.open(gif_path)

    # In palettierten Modus zwingen
    img = img.convert("P")

    width, height = img.size
    pixels = list(img.getdata())
    palette = img.getpalette()  # [R,G,B,R,G,B,...]

    # Palette in RGB-Tupel umwandeln
    palette_rgb = [
        (palette[i], palette[i+1], palette[i+2])
        for i in range(0, len(palette), 3)
    ]

    # Ungenutzte Paletteinträge entfernen
    used_indices = sorted(set(pixels))
    palette_rgb = [palette_rgb[i] for i in used_indices]

    # Mapping alt → neu
    index_map = {old: new for new, old in enumerate(used_indices)}
    pixels = [index_map[p] for p in pixels]

    # ---------- C-Ausgabe ----------
    print(f"// Bildgröße: {width} x {height}\n")

    print(f"const unsigned char palette[{len(palette_rgb)}][3] = {{")
    for r, g, b in palette_rgb:
        print(f"    {{{r}, {g}, {b}}},")
    print("};\n")

    print(f"const unsigned char pixels[{height}][{width}] = {{")
    for y in range(height):
        row = pixels[y*width:(y+1)*width]
        print("    {" + ", ".join(map(str, row)) + "},")
    print("};")

if __name__ == "__main__":
    gif_to_c_arrays("../mister_logo.png")