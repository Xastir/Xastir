typedef struct _shpinfo{
    char *filename;
    struct Node* root;
}shpinfo;

void init_shp_hash(int clobber);
void add_shp_to_hash(char *filename,SHPHandle sHP);
void build_rtree(struct Node **root, SHPHandle sHP);
shpinfo *get_shp_from_hash(char *filename);

