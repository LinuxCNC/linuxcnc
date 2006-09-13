char *DisplayInfo(int Type, int Offset);
char *DisplayArithmExpr(char *Expr, int NumCarMax);
void DrawElement(GdkPixmap * DrawPixmap, int x, int y, int Width, int Height,
    StrElement Element, char DrawForToolBar);
void DrawElementFile(FILE *File, int x, int y, int Width, int Height,
    StrElement Element);
void GetTheSizesForRung();
void DrawRungs();
void DrawCurrentSection(void);
