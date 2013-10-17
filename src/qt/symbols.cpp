#include "symbols.h"
#include <QFile>
#include <QtDebug>
#include <QMessageBox>
#include <QPainter>
#include "colors.h"

int  icon_outline_style = 0;
QHash<QString,Symbol *> symbol_data;


Symbol::Symbol() : pix(20,20), pix_mask(20,20), pix_mask_old(20,20)
{
    pix.fill(QColor(255,255,255,0));
    pix_mask.fill();
    pix_mask_old.fill();
}


void load_pixmap_symbol_file(char *filename, int reloading) {
    QFile *f;
//    char filen[500];
    char line[100];
    char table_char;
    char symbol_char;
    int done;
    char pixels[400];
    char orient;

    //symbols_loaded = 0;
    table_char = '\0';
    symbol_char = '\0';
    done = 0;

    f = new QFile(filename);
    if (f->open(QIODevice::ReadOnly)) {
        while (!f->atEnd() && !done) {
            f->readLine(line,100);
            if (strncasecmp("TABLE ",line,6)==0) {
                table_char=line[6];
                /*fprintf(stderr,"TABLE %c\n",table_char);*/
            } else {
                if (strncasecmp("DONE",line,4)==0) {
                    done=1;
                    /*fprintf(stderr,"DONE\n");*/
                } else {
                    if (strncasecmp("APRS ",line,5)==0) {
                        symbol_char=line[5];
                        if (strlen(line)>=20 && line[19] == 'l')     // symbol with orientation ?
                            orient = 'l';   // should be 'l' for left
                        else
                            orient = ' ';
                        read_symbol_from_file(f, pixels, table_char);                      // read pixels for one symbol
                        insert_symbol(table_char,symbol_char,pixels,270,orient,reloading); // always have normal orientation
                        if (orient == 'l') {
                            insert_symbol(table_char,symbol_char,pixels, 0,'u',reloading); // create other orientations
                            insert_symbol(table_char,symbol_char,pixels, 90,'r',reloading);
                            insert_symbol(table_char,symbol_char,pixels,180,'d',reloading);
                        }
                    }
                }
            }
        }
        f->close();
    } else {
        qDebug() << "Error opening symbol file " << filename <<": " << f->errorString();
        QMessageBox::warning(NULL, "Error Opening Symbol File", "Unable to open APRS Symbols");
    }
    delete f;
}

// read pixels from file, speeding it up by smart ordering of switches
void read_symbol_from_file(QFile *f, char *pixels, char table_char) {
    int x,y;
    int color;
    char line[100];
    char pixels_copy[400];
    char *p,*q;
    unsigned char a, b, c;

    for (y=0;y<20;y++) {
        f->readLine(line,100);
        for (x=0;x<20;x++) {
            switch (line[x]) {
                case('.'):       // transparent
                    color=0xff;
                    break;
                case('q'):       // #000000  black   0%
                    color=0x51;
                    break;
                case('m'):       // #FFFFFF  white 100%
                    color=0x4d;
                    break;
                case('c'):       // #CCCCCC  gray80 80%
                    color=0x43;
                    break;
                case('j'):       // #EE0000  red2
                    color=0x4a;
                    break;
                case('h'):       // #00BFFF  Deep sky blue
                    color=0x48;
                    break;
                case('l'):       // #0000CD  mediumblue
                    color=0x4c;
                    break;
                case('k'):       // #00CD00  green3
                    color=0x4b;
                    break;
                case('g'):       // #00008B  blue4
                    color=0x47;
                    break;
                case('#'):       // #FFFF00  yellow
                    color=0x40;
                    break;
                case('p'):       // #454545  gray27 27%
                    color=0x50;
                    break;
                case('~'):       // used in the last two symbols in the file
                    color=0xff;  // what should it be? was transparent before...
                    break;
                case('i'):       // #006400  Dark Green
                    color=0x49;
                    break;
                case('n'):       // #878787  gray53 52%
                    color=0x4e;
                    break;
                case('a'):       // #CD6500  darkorange2
                    color=0x41;
                    break;
                case('o'):       // #5A5A5A  gray59 35%
                    color=0x4f;
                    break;
                case('f'):       // #CD3333  brown3
                    color=0x46;
                    break;
                case('b'):       // #A020F0  purple
                    color=0x42;
                    break;
                case('e'):       // #FF4040  brown1
                    color=0x45;
                    break;
                case('d'):       // #CD0000  red3
                    color=0x44;
                    break;
                case('r'):       //          LimeGreen  DK7IN: saw this in the color definitions...
                    color=0x52;                         // so we could use it
                    break;
                default:
                    color=0xff;
                    break;
            }
            pixels[y*20+x] = (char)(color);
        }
    }

    // Create outline on icons, if needed
    // Do not change the overlays and "number" tables
    if((icon_outline_style != 0) && (table_char != '~') && (table_char != '#'))
    {
        switch(icon_outline_style) {
        case 1:  color = 0x51; // Black
                 break;
        //case 2:  color = 0x43; // Grey 80%
        case 2:  color = 0x4e; // Grey 52%
                 break;
        case 3:  color = 0x4d; // White
                 break;
        default: color = 0xff; // Transparent
                 break;
        }

        p = pixels;
        q = &pixels_copy[0];

        for (y=0;y<20;y++) {
            for (x=0;x<20;x++) {
                *q = *p; // copy current color

                // If transparent see if the pixel is on the edge
                if(*q == (char) 0xff)
                {
                    //check if left or right is none transparent
                    b = c = 0xff;

                    // left (left only possible if x > 0)
                    if(x > 0)
                        b = p[-1];
                    // right (right only possible if x < 19)
                    if(x < 19)
                        c = p[+1];

                    // if non-transparent color is found change pixel
                    // to outline color
                    if((b != (unsigned char) 0xff)
                            || (c != (unsigned char) 0xff)) {
                        // change to icon outline color
                        *q = color;
                    }

                    if((y > 0) && (*q == (char) 0xff)) {
                        //check if left-up, up or right-up is none transparent
                        //"up" only possible if y > 0
                        a = b = c = 0xff;

                        // left-up (left only possible if x > 0)
                        if(x > 0)
                            a = p[-21];
                        // up
                        b = p[-20];
                        // right-up (right only possible if x < 19)
                        if(x < 19)
                            c = p[-19];

                        // if non-transparent color is found change pixel
                        // to outline color
                        if((a != (unsigned char) 0xff)
                                || (b != (unsigned char) 0xff)
                                || (c != (unsigned char) 0xff)) {
                            // change to icon outline color
                            *q = color;
                        }
                    }

                    if((y < 19) && (*q == (char) 0xff)) {
                        //check if left-down, down or right-down is none transparent
                        //"down" only possible if y < 19
                        a = b = c = 0xff;

                        // left-down (left only possible if x > 0)
                        if(x > 0)
                            a = p[+19];
                        // down
                        b = p[+20];
                        // right-down (right only possible if x < 19)
                        if(x < 19)
                            c = p[+21];

                        // if non-transparent color is found change pixel
                        // to outline color
                        if((a != (unsigned char) 0xff)
                                || (b != (unsigned char) 0xff)
                                || (c != (unsigned char) 0xff)) {
                            // change to icon outline color
                            *q = color;
                        }
                    }
                }

                p++;
                q++;
            }
        }
        memcpy(pixels, pixels_copy, 400);
    }
}

// add a symbol to the end of the symbol table.
//
// Here we actually draw the pixels into the SymbolData struct,
// which contains separate Pixmap's for the icon, the transparent
// background, and the ghost image.
//
void insert_symbol(char table, char symbol, char *pixel, int deg, char orient, int reloading) {
    int x,y,idx,old_next,color,last_color,last_gc2;
    Symbol *symbolItem;

    // first time loading, -> create pixmap...
    // when reloading -> reuse already created pixmaps...
    if(reloading == 0) {
        symbolItem = new Symbol;
    } else
    {
        // XXX find the symbol and set the pointer to it
    }

    QPainter pixPaint(&(symbolItem->pix));
    QPainter pixMaskPaint(&(symbolItem->pix_mask));
    QPainter pixMaskOldPaint(&(symbolItem->pix_mask_old));

    old_next=0;
    last_color = -1;    // Something bogus
    last_gc2 = -1;      // Also bogus

    for (y=0;y<20;y++) {
        for (x=0;x<20;x++) {
            switch (deg) {
            case(0):
                idx = 20* (19-x) +   y;
                break;
            case(90):
                idx = 20*   y    + (19-x);
                break;
            case(180):
                idx = 20* (19-x) + (19-y);
                break;
            default:
                idx = 20*   y    +   x;
                break;
            }
            color = (int)(pixel[idx]);
            if (color<0)
                color = 0xff;

            // Change to new color only when necessary.  We use two different
            // GC's here, one for the main icon pixmap, and one for the symbol
            // mask and ghost layer.
            if (color != last_color) {
                pixPaint.setPen(colors[color]);
                last_color = color;
            }
            pixPaint.drawPoint(x,y);


            // Create symbol mask
            if (color != 0xff) {
                if (last_gc2 != 1) {
                    pixMaskPaint.setPen(QColor("black"));
                    pixMaskOldPaint.setPen(QColor("black"));
                    last_gc2 = 1;
                }
            }
            else {
                if (last_gc2 != 0) {
                    pixMaskPaint.setPen(QColor("white"));
                    pixMaskOldPaint.setPen(QColor("white"));
                    last_gc2 = 0;
                }
            }


            pixMaskPaint.drawPoint(x,y);

            // Create ghost symbol mask by setting every 2nd bit
            // to transparent
            old_next++;
            if (old_next>1) {
                old_next=0;
                if (last_gc2 != 0) {
                    pixMaskOldPaint.setPen(QColor("white"));
                    last_gc2 = 0;
                }
            }
            pixMaskOldPaint.drawPoint(x,y);
        }
        old_next++;    // shift one bit every scan line for ghost image
        if (old_next>1)
            old_next=0;
    }
    //symbolItem->pix.setMask(symbolItem->pix_mask);
    symbolItem->table = table;
    symbolItem->symbol = symbol;
    symbolItem->orient = orient;
    char key[4];
    key[0] = table;
    key[1] = symbol;
    key[2] = orient;
    key[3] = 0;
    symbol_data.insert(QString(key), symbolItem);
}
