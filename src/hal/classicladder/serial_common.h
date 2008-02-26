
char SerialOpen( char * SerialPortName, int Speed );
void SerialClose( );
void SerialSend( char *Buff, int BuffLength );
void SerialSetResponseSize( int Size, int TimeOutResp );
int SerialReceive( char * Buff, int MaxBuffLength );
void SerialFlush( void );

