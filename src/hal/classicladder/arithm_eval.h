
#define arithmtype int

int IdentifyVariable(char *StartExpr, int *ResType, int *ResOffset);
int EvalCompare(char *CompareString);
void MakeCalc(char *CalcString, int VerifyMode);
arithmtype AddSub(void);
char *VerifySyntaxForEvalCompare(char *StringToVerify);
char *VerifySyntaxForMakeCalc(char *StringToVerify);
