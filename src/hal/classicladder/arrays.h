#ifdef RTAPI
int ClassicLadderAllocAll(int compId, plc_sizeinfo_s * SizesInfos);
#else
int ClassicLadderAllocAll(int compId);
#endif
void ClassicLadderFreeAll(int compId);
