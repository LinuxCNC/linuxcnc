extern int ShmemId;

// Pointers to SHMEM global data.
extern StrInfosGene *InfosGene;
extern StrRung *RungArray;
extern StrTimer *TimerArray;
extern StrMonostable *MonostableArray;
extern StrArithmExpr *ArithmExpr;
extern StrSection *SectionArray;
#ifdef SEQUENTIAL_SUPPORT
extern StrSequential *Sequential;
#endif
extern int *VarWordArray;
extern TYPE_FOR_BOOL_VAR *VarArray;

#ifndef MODULE
extern StrDatasForBase CorresDatasForBase[3];
extern char LadderDirectory[400];
extern char TmpDirectory[400];
#ifdef GTK_INTERFACE
extern StrEditRung EditDatas;
extern StrArithmExpr *EditArithmExpr;
#endif
#ifdef __GTK_H__
extern GdkPixmap *pixmap;
extern GtkWidget *drawing_area;
#endif
#endif
