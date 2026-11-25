#ifndef CLASSICLADDER_EDIT_COPY_H
#define CLASSICLADDER_EDIT_COPY_H

void StartOrMotionPartSelection(double x,double y, char StartToClick);
void EndPartSelection( );
void GetSizesOfTheSelectionToCopy( int * pSizeX, int * pSizeY );
char GetIsOutputEleLastColumnSelection( );
void CopyNowPartSelected( double x,double y );



#endif
