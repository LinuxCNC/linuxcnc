void DrawSeqStep(GdkPixmap * DrawPixmap, int x, int y, int Size,
    StrStep * pStep, char DrawForToolBar);
void DrawSeqTransition(GdkPixmap * DrawPixmap, int x, int y, int Size,
    StrTransition * pTransi, char DrawForToolBar);

void DrawSequentialPage(int PageNbr);

void DrawSeqElementForToolBar(GdkPixmap * DrawPixmap, int x, int y, int Size,
    int NumElement);
