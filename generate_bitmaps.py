import sys

def make_bitmap(name, lines):
    # lines is a list of 32 strings of 32 chars
    res = f"const unsigned char {name}_bmp[] PROGMEM = {{\n"
    bytes_arr = []
    for line in lines:
        for i in range(0, 32, 8):
            chunk = line[i:i+8]
            # XBM format reverses bits, but let's just do standard MSB first for simplicity, or LSB for standard XBM.
            # M5GFX drawBitmap expects MSB first or we can use drawXBitmap if LSB first.
            val = 0
            for j, c in enumerate(chunk):
                if c != ' ':
                    val |= (1 << j) # LSB first for XBM
            bytes_arr.append(f"0x{val:02x}")
    
    res += ", ".join(bytes_arr) + "\n};\n"
    return res

sun = [
    "                                ",
    "                                ",
    "        #              #        ",
    "         #            #         ",
    "          #          #          ",
    "                                ",
    "             ######             ",
    "           ##      ##           ",
    "          #          #          ",
    "  ##     #            #     ##  ",
    "         #            #         ",
    "         #            #         ",
    "          #          #          ",
    "           ##      ##           ",
    "             ######             ",
    "                                ",
    "          #          #          ",
    "         #            #         ",
    "        #              #        ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                "
]

cloud = [
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "            ########            ",
    "          ##        ##          ",
    "         #            #         ",
    "        #              #        ",
    "     ###                #       ",
    "   ##                    #      ",
    "  #                       ##    ",
    " #                          #   ",
    " #                           #  ",
    " #                           #  ",
    "  #                         #   ",
    "   ##                     ##    ",
    "     #####################      ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                "
]

rain = cloud[:19] + [
    "    #      #      #      #      ",
    "    #      #      #      #      ",
    "   #      #      #      #       ",
    "   #      #      #      #       ",
    "  #      #      #      #        ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                "
]

snow = cloud[:19] + [
    "                                ",
    "     #      #      #      #     ",
    "    ###    ###    ###    ###    ",
    "     #      #      #      #     ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                "
]

thunder = cloud[:19] + [
    "           #                    ",
    "          #                     ",
    "         #                      ",
    "       #####                    ",
    "         #                      ",
    "        #                       ",
    "       #                        ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                "
]

with open("lib/Display/weather_bitmaps.h", "w") as f:
    f.write("#pragma once\n\n")
    f.write(make_bitmap("icon_sun", sun))
    f.write(make_bitmap("icon_cloud", cloud))
    f.write(make_bitmap("icon_rain", rain))
    f.write(make_bitmap("icon_snow", snow))
    f.write(make_bitmap("icon_thunder", thunder))
