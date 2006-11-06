#define MODE_MODIFY 0
#define MODE_ADD 1
#define MODE_INSERT 2

int TextToNumber(char * text,int ValMin,int ValMaxi,int *ValFound);
int TextParserForAVar( char * text, int * VarTypeFound, int * VarOffsetFound, int * pNumberOfChars, char PartialNames );
void SaveElementProperties(void);
void InitBufferRungEdited( StrRung * pRung );
int GetNbrRungsDefined(void);
int FindFreeRung(void);
void AddRung(void);
void InsertRung(void);
void ModifyCurrentRung(void);
void DeleteCurrentRung(void);
void CancelRungEdited(void);
void ApplyRungEdited(void);
void EditElementInRung(double x,double y);
void EditElementInThePage(double x,double y);

