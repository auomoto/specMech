/*------------------------------------------------------------------------------
NMEA checksum computations
	Append the NMEA style checksums to the presented character string.
------------------------------------------------------------------------------*/

#include "globals.h"

/*------------------------------------------------------------------------------
void checksum_NMEA(char *str)
	Append checksum to a string
------------------------------------------------------------------------------*/
void checksum_NMEA(char *str)
{
	const char format_CSM[] = "*%02X\r\n";

	char chksum[6];
	uint8_t i, nchar, checksum;

	checksum = 0;
	nchar = strlen(str);
	for (i = 1; i < nchar; i++) {
		checksum ^= str[i];
	}
	sprintf(chksum, format_CSM, checksum);
	strcat(str, chksum);

}

/*------------------------------------------------------------------------------
void format_ERR(char *str)
	Returns an error line
------------------------------------------------------------------------------*/
void format_ERR(char *str)
{

	const char errformat[] = "$S%dERR";

	sprintf(str, errformat, get_specID());
	checksum_NMEA(str);

}
