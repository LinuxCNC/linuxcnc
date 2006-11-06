void InitVars(void);
int ReadVar(int TypeVar,int Offset);
void WriteVar(int TypeVar,int NumVar,int Value);

/* these are only useful for the MAT-connected version */
void DoneVars(void);
void CycleStart(void);
void CycleEnd(void);
