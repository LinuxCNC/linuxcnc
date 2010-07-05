
char SerialOpen( char * SerialPortName, int Speed );
void SerialClose( );
void SerialSend( char *Buff, int BuffLength );
void SerialSetResponseSize( int Size, int TimeOutResp );
int SerialReceive( char * Buff, int MaxBuffLength, int TimeOutResp );
void SerialFlush( void );

