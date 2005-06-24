#ifdef NO_GTK_STYLE_GET_FONT
#define gtk_style_get_font(style)	(style)->font
#endif

char *DisplayInfo(int Type, int Offset);
char *DisplayArithmExpr(char *Expr, int NumCarMax);
void DrawElement(GdkPixmap * DrawPixmap, int x, int y, int Width, int Height,
    StrElement Element, char DrawForToolBar);
void GetTheSizesForRung();
void DrawRungs();
void DrawCurrentSection(void);
