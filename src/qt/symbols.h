#ifndef SYMBOLS_H
#define SYMBOLS_H

#include <QFile>
#include <QPixmap>
#include <QBitmap>
#include <QHash>

class Symbol
{
public:
    QPixmap pix;
    QBitmap pix_mask;
    QBitmap pix_mask_old;
    char table;                 // table character
    char symbol;                // symbol character
    char orient;                // orientation of the symbol, one of ' ',  'l','r','u','d'

    Symbol();
};

extern QHash<QString,Symbol*> symbol_data;

void load_pixmap_symbol_file(char *filename, int reloading);
void read_symbol_from_file(QFile *f, char *pixels, char table_char);
void insert_symbol(char table, char symbol, char *pixel, int deg, char orient, int reloading);
#endif // SYMBOLS_H
