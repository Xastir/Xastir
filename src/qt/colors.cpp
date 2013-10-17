#include "colors.h"

QVector<QColor> colors(0x100);


void initializeColors( void )
{
    // Allocate colors
    // Note that the names here are the ones given in xastir.rgb
    colors[0x00] = QColor("DarkGreen");  // was darkgreen (same)
    colors[0x01] = QColor("purple");
    colors[0x02] = QColor("DarkGreen");  // was darkgreen (same)
    colors[0x03] = QColor("cyan");
    colors[0x04] = QColor("brown");
    colors[0x05] = QColor("plum");       // light magenta
    colors[0x06] = QColor("orange");
    colors[0x07] = QColor("darkgray");
    colors[0x08] = QColor("black");      // Foreground font color
    colors[0x09] = QColor("blue");
    colors[0x0a] = QColor("green");              // PHG (old)
    colors[0x0b] = QColor("mediumorchid"); // light purple
    colors[0x0c] = QColor("red");
    colors[0x0d] = QColor("magenta");
    colors[0x0e] = QColor("yellow");
    colors[0x0f] = QColor("white");              //
    colors[0x10] = QColor("black");
    colors[0x11] = QColor("black");
    colors[0x12] = QColor("black");
    colors[0x13] = QColor("black");
    colors[0x14] = QColor("lightgray");
    colors[0x15] = QColor("magenta");
    colors[0x16] = QColor("mediumorchid"); // light purple
    colors[0x17] = QColor("lightblue");
    colors[0x18] = QColor("purple");
    colors[0x19] = QColor("orange2");    // light orange
    colors[0x1a] = QColor("SteelBlue");
    colors[0x20] = QColor("white");

    // Area object colors.  Order must not be changed. If beginning moves,
    // update draw_area and draw_map.
    // High
    colors[0x21] = QColor("black");   // AREA_BLACK_HI
    colors[0x22] = QColor("blue");    // AREA_BLUE_HI
    colors[0x23] = QColor("green");   // AREA_GREEN_HI
    colors[0x24] = QColor("cyan3");    // AREA_CYAN_HI
    colors[0x25] = QColor("red");     // AREA_RED_HI
    colors[0x26] = QColor("magenta"); // AREA_VIOLET_HI
    colors[0x27] = QColor("yellow");  // AREA_YELLOW_HI
    colors[0x28] = QColor("gray35");  // AREA_GRAY_HI
    // Low
    colors[0x29] = QColor("gray27");   // AREA_BLACK_LO
    colors[0x2a] = QColor("blue4");    // AREA_BLUE_LO
    colors[0x2b] = QColor("green4");   // AREA_GREEN_LO
    colors[0x2c] = QColor("cyan4");    // AREA_CYAN_LO
    colors[0x2d] = QColor("red4");     // AREA_RED_LO
    colors[0x2e] = QColor("magenta4"); // AREA_VIOLET_LO
    colors[0x2f] = QColor("yellow4");  // AREA_YELLOW_LO
    colors[0x30] = QColor("gray53"); // AREA_GRAY_LO

    colors[0x40] = QColor("yellow");     // symbols ...
    colors[0x41] = QColor("darkorange");
    colors[0x42] = QColor("purple");
    colors[0x43] = QColor("darkgray");
    colors[0x44] = QColor("red");
    colors[0x45] = QColor("brown");
    colors[0x46] = QColor("lightbrown");
    colors[0x47] = QColor("blue");
    colors[0x48] = QColor("deepblue");
    colors[0x49] = QColor("DarkGreen");
    colors[0x4a] = QColor("lightred");
    colors[0x4b] = QColor("lawngreen");
    colors[0x4c] = QColor("slateblue");
    colors[0x4d] = QColor("white");
    colors[0x4e] = QColor("gray");
    colors[0x4f] = QColor("lightgray");
    colors[0x50] = QColor("lightslategray");
    colors[0x51] = QColor("black");      // ... symbols

    colors[0x52] = QColor("limegreen");  // PHG, symbols

    colors[0xfe] = QColor("pink");

    // map solid colors
    colors[0x60] = QColor("HotPink");
    colors[0x61] = QColor("RoyalBlue");
    colors[0x62] = QColor("orange3");
    colors[0x63] = QColor("yellow3");
    colors[0x64] = QColor("ForestGreen");
    colors[0x65] = QColor("DodgerBlue");
    colors[0x66] = QColor("cyan2");
    colors[0x67] = QColor("plum2");
    colors[0x68] = QColor("MediumBlue"); // was blue3 (the same!)
    colors[0x69] = QColor("gray86");

    // These colors added to make it possible to color local shapefile tiger
    //  maps similar to on-line ones.
    colors[0x70] = QColor("RosyBrown2");
    colors[0x71] = QColor("gray81");
    colors[0x72] = QColor("tgr_park_1");
    colors[0x73] = QColor("tgr_city_1");
    colors[0x74] = QColor("tgr_forest_1");
    colors[0x75] = QColor("tgr_water_1");
    colors[0x08] = QColor("black");
    colors[0x0c] = QColor("red");
    colors[0xff] = QColor(0,0,0,0);


}
