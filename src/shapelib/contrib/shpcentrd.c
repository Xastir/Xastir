/******************************************************************************
 * Copyright (c) 1999, Carl Anderson
 *
 * this code is based in part on the earlier work of Frank Warmerdam
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ******************************************************************************
 *
 * shpcentrd.c  - compute XY centroid for complex shapes 
 *			and create a new SHPT_PT file of then
 * 			specifically undo compound objects but not complex ones
 *
 *
 * $Log$
 * Revision 1.3  2010/07/11 07:24:37  we7u
 * Fixing multiple minor warnings with Shapelib.  Still plenty left.
 *
 * Revision 1.2  2007/07/25 15:45:27  we7u
 * Adding includes necessary for warning-free compiles.
 *
 * Revision 1.1  2006/11/10 21:48:10  tvrusso
 * Add shapelib as an internal library, and use it if we don't find an external
 * one.
 *
 * Make a loud warning if we do so, because the result of this is that we'll
 * have a bigger executable.
 *
 * This commit is bigger than it needs to be, because it includes all of
 * shapelib, including the contrib directory.
 *
 * Added an automake-generated Makefile for this thing.
 *
 * Builds only a static library, and calls it "libshape.a" instead of
 * "libshp.a" so that if we use ask to use the static one while there is
 * also an external one installed, the linker doesn't pull in the shared
 * library one unbidden.
 *
 * This stuff can be tested on a system with libshp installed by configuring with
 * "--without-shapelib"
 *
 * I will be removing Makefile.in because it's not supposed to be in CVS.  My
 * mistake.
 *
 * Revision 1.2  1999/05/26 02:56:31  candrsn
 * updates to shpdxf, dbfinfo, port from Shapelib 1.1.5 of dbfcat and shpinfo
 *
 *
 * 
 */


/* the centroid is defined as
 *
 *      Cx = sum (x dArea ) / Total Area
 *  and
 *      Cy = sum (y dArea ) / Total Area
 */           

#include "shapefil.h"
#include "shpgeo.h"
#include <string.h>

int main( int argc, char ** argv )

{
    SHPHandle	old_SHP, new_SHP;
    DBFHandle   old_DBF, new_DBF;
    int		nShapeType, nEntities, i;
//    int     nVertices, nParts, *panParts, iPart;
//    double	*padVertices, adBounds[4];
//    const char 	*pszPlus;
//    DBFFieldType  idfld_type;
//    int		idfld, nflds;
//    char	kv[257] = "";
//    char	idfldName[120] = "";
//    char	fldName[120] = "";
//    char	shpFileName[120] = "";
//    char	dbfFileName[120] = "";
//    double	apeture[4];
    char	*DBFRow = NULL;
//    int		Cpan[2] = { 0,0 };
    int		byRing = 1;
    PT		Centrd;
    SHPObject	*psCShape, *cent_pt;


    if( argc < 3 )
    {
	printf( "shpcentrd shp_file new_shp_file\n" );
	exit( 1 );
    }
    
    old_SHP = SHPOpen (argv[1], "rb" );
    old_DBF = DBFOpen (argv[1], "rb");
    if( old_SHP == NULL || old_DBF == NULL )
    {
	printf( "Unable to open old files:%s\n", argv[1] );
	exit( 1 );
    }

    SHPGetInfo( old_SHP, &nEntities, &nShapeType, NULL, NULL );
    new_SHP = SHPCreate ( argv[2], SHPT_POINT ); 
    
    new_DBF = DBFCloneEmpty (old_DBF, argv[2]);
    if( new_SHP == NULL || new_DBF == NULL )
    {
	printf( "Unable to create new files:%s\n", argv[2] );
	exit( 1 );
    }

    DBFRow = (char *) malloc ( (old_DBF->nRecordLength) + 15 );


#ifdef 	DEBUG
    printf ("ShpCentrd using shpgeo \n");
#endif

    for( i = 0; i < nEntities; i++ )
    {
//	int		res ;
 
	psCShape = SHPReadObject( old_SHP, i );

        if ( byRing == 1 ) {
          int 	ring;
          for ( ring = 0; ring < psCShape->nParts; ring ++ ) {
	    SHPObject 	*psO;
	    psO = SHPClone ( psCShape, ring,  ring + 1 );

            Centrd = SHPCentrd_2d ( psO ); 

            cent_pt = SHPCreateSimpleObject ( SHPT_POINT, 1, 
        	(double*) &(Centrd.x), (double*) &(Centrd.y), NULL ); 

            SHPWriteObject ( new_SHP, -1, cent_pt ); 
                        
            memcpy ( DBFRow, DBFReadTuple ( old_DBF, i ),
		 old_DBF->nRecordLength );
            DBFWriteTuple ( new_DBF, new_DBF->nRecords, DBFRow );            

            SHPDestroyObject ( cent_pt );

	    SHPDestroyObject ( psO );
           }

          }
        else {
        
          Centrd = SHPCentrd_2d ( psCShape ); 

          cent_pt = SHPCreateSimpleObject ( SHPT_POINT, 1, 
        	(double*) &(Centrd.x), (double*) &(Centrd.y), NULL ); 

          SHPWriteObject ( new_SHP, -1, cent_pt ); 
                        
          memcpy ( DBFRow, DBFReadTuple ( old_DBF, i ),
		 old_DBF->nRecordLength );
          DBFWriteTuple ( new_DBF, new_DBF->nRecords, DBFRow );            

          SHPDestroyObject ( cent_pt );
         }
    }

    SHPClose( old_SHP );
    SHPClose( new_SHP );
    DBFClose( old_DBF );
    DBFClose( new_DBF );
    printf ("\n");
    return(0);
}

