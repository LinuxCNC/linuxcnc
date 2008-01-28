#define DRAW_NORMAL 0
#define DRAW_FOR_TOOLBAR 1
#define DRAW_FOR_PRINT 2

char * DisplayInfo(int Type, int Offset);
char * DisplayArithmExpr(char * Expr,int NumCarMax);
void DrawTextWithOffsetGTK2( GdkPixmap * DrawPixmap, GdkGC * GcRef, int BaseX, int BaseY, int Width, int Height, char * Text, int BorderOffset );
void DrawTextGTK2( GdkPixmap * DrawPixmap, GdkGC * GcRef, int BaseX, int BaseY, int Width, int Height, char * Text );
void DrawCommonElementForToolbar(GdkPixmap * DrawPixmap,int x,int y,int Size,int NumElement);
void DrawElement(GdkPixmap * DrawPixmap,int x,int y,int Width,int Height,StrElement Element,char DrawingOption);
void GetTheSizesForRung();
void DrawRung(GdkPixmap * DrawPixmap, StrRung * Rung, int PosiY, int BlockWidth, int BlockHeight, char DrawingOption);
void DrawRungs();
void DrawCurrentSection( void );

