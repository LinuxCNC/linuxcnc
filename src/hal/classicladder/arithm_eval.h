
#define arithmtype int


int IdentifyVarIndexedOrNot(char * StartExpr,int * ResType,int * ResOffset, int * ResIndexType,int * ResIndexOffset);
int EvalCompare(char * CompareString);
void MakeCalc(char * CalcString,int VerifyMode);
arithmtype AddSub(void);
char * VerifySyntaxForEvalCompare(char * StringToVerify);
char * VerifySyntaxForMakeCalc(char * StringToVerify);


