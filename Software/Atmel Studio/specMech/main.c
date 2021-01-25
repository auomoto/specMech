/*------------------------------------------------------------------------------
specMech.c
	BOSS motion controller board based on an ATMega4809 implemented on a
	Microchip Curiosity Nano
------------------------------------------------------------------------------*/
#define F_CPU		3333333UL
#define VERSION		"2021-01-24"
#define	YES				1
#define	NO				0
#define GREATERPROMPT	0	// Standard return prompt >
#define EXCLAIMPROMPT	1	// No REBOOT ACK prompt !
#define ERRORPROMPT		2	// Generate error line, then >
#define CVALUESIZE		41	// Maximum length of a command value string
#define CIDSIZE			9	// Maximum length of a command ID string
#define CSTACKSIZE		10	// Number of stacked up commands allowed

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdio.h>		// sprintf
#include <string.h>		// for strcpy, strlen

#include "ports.c"			// ATMega4809 ports
#include "led.c"			// On-board LED
#include "specid.c"			// Spectrograph ID jumper
#include "twi.c"			// I2C
#include "mcp23008.c"		// MCP23008 port expander
#include "pneu.c"			// Pneumatic valves and sensors
#include "ads1115.c"		// ADS1115 ADC
#include "ad590.c"			// AD590 temperature sensors
#include "mcp9808.c"		// MCP9808 sensor
#include "temperature.c"	// AD590 and MCP9808 sensors
#include "humidity.c"		// Honeywell HiH-4031
#include "ionpump.c"		// Read the ion pump vacuum
#include "usart.c"			// Serial (RS232) communications
#include "fram.c"			// FRAM memory
#include "ds3231.c"			// Day-time clock
#include "nmea.c"			// GPS style output
#include "oled.c"			// Newhaven OLED display
#include "eeprom.c"			// ATMega4809 eeprom
#include "wdt.c"			// Watchdog timer used only for reboot function

// Function Prototypes
void Xcommands(void);
void commands(void);
void echo_cmd(char *str);
uint8_t isaletter(char);
void parse_cmd(char*, uint8_t);
uint8_t report(uint8_t);
void send_prompt(uint8_t);
uint8_t set(uint8_t);

// Globals
extern USARTBuf		// These are declared in usart.c
	send0_buf, send1_buf, send3_buf,
	recv0_buf, recv1_buf, recv3_buf;

typedef struct {
	char cverb,				// Single character command
	cobject,			// Single character object
	cvalue[CVALUESIZE],	// Input value string for object
	cid[CIDSIZE];		// Command ID string
} ParsedCMD;

ParsedCMD pcmd[CSTACKSIZE];	// Split the command line into its parts

int main(void)
{

	init_PORTS();
	init_LED();
	init_SPECID();
	init_TWI();
	init_PNEU();
	init_USART();
	init_OLED(0);
	init_EEPROM();
	sei();

	for (;;) {
		if (recv0_buf.done) {
			recv0_buf.done = NO;
			commands();
		}
	}

}

void commands(void)
{

	char cmdline[BUFSIZE];			// BUFSIZE is the size of the ring buffer
	char verb, object;
	uint8_t i, prompt_flag = GREATERPROMPT;
	static uint8_t rebootnack = 1;
	static uint8_t cstack = 0;		// Index for pcmd

	// Copy the command string to cmdline
	for (i = 0; i < BUFSIZE; i++) {
		cmdline[i] = recv0_buf.data[recv0_buf.tail];
		recv0_buf.tail = (recv0_buf.tail + 1) % BUFSIZE;
		if (cmdline[i] == '\0') {
			break;
		}
	}

	// Check if rebooted
	if (rebootnack) {
		if (cmdline[0] != '!') {
			send_prompt(EXCLAIMPROMPT);		
			return;
		} else {
			send_prompt(GREATERPROMPT);
			rebootnack = 0;
			return;
		}
	}

	echo_cmd(cmdline);
	parse_cmd(cmdline, cstack);
	verb = pcmd[cstack].cverb;
	object = pcmd[cstack].cobject;
	switch (verb) {
		case 'c':				// close
			prompt_flag = pneu_close(object);
			break;

		case 'o':				// open
			prompt_flag = pneu_open(object);
			break;

		case 'r':				// Report
			prompt_flag = report(cstack);
			break;

		case 's':
			prompt_flag = set(cstack);
			break;

		case 'R':				// Reboot
			_delay_ms(100);		// Avoid finishing the command loop before reboot
			reboot();
			return;

		default:
			prompt_flag = ERRORPROMPT;
			break;			
	}

	cstack = (cstack + 1) % CSTACKSIZE;
	send_prompt(prompt_flag);

}

void echo_cmd(char *cmdline)
{

	char format_CMD[] = "$S%dCMD,%s";
	char strbuf[BUFSIZE+10];		// Add $SXCMD, and *HH checksum

		// Format and echo the command line
	sprintf(strbuf, format_CMD, get_specID(), cmdline);
	checksum_NMEA(strbuf);
	send_USART(0, (uint8_t*) strbuf, strlen(strbuf));

}

void parse_cmd(char *ptr, uint8_t n)
{

	uint8_t i;

		// Clear the command parts
	pcmd[n].cverb = '?';
	pcmd[n].cobject = '?';
	pcmd[n].cvalue[0] = '\0';
	pcmd[n].cid[0] = '\0';

		// Find the verb
	while (!isaletter(*ptr)) {
		if (*ptr == '\0') {
			return;
		}
		ptr++;
	}
	pcmd[n].cverb = *ptr++;

		// Find the object
	while (!isaletter(*ptr)) {
		if (*ptr == '\0') {
			return;
		}
		ptr++;
	}
	pcmd[n].cobject = *ptr++;

		// Get the value, if there is one
	for (i = 0; i < CVALUESIZE; i++) {
		if (*ptr == '\0') {
			pcmd[n].cvalue[i] = '\0';
			return;
		}
		if (*ptr == ';') {
			pcmd[n].cvalue[i] = '\0';
			break;
		}
		pcmd[n].cvalue[i] = *ptr++;
	}

		// get the command ID if there is one
	ptr++;
	for (i = 0; i < CIDSIZE; i++) {
		if (*ptr == '\0') {
			pcmd[n].cid[i] = '\0';
			return;
		}
		pcmd[n].cid[i] = *ptr++;
	}	

	return;

}

uint8_t isaletter(char c)
{

	if (c >= 'a' && c <= 'z') {
		return(1);
	}

	if (c >= 'A' && c <= 'Z') {
		return(1);
	}

	return(0);

}

/*------------------------------------------------------------------------------
uint8_t report(char *ptr)
	Report status, including reading sensors

	Input
		*ptr - Command line pointer. Incremented to find object to report

	Output
		Prints NMEA formatted output to the serial port.

	Returns
		0 - GREATERPROMPT on success
		1 - ERRORPROMPT on error (invalid command noun)
------------------------------------------------------------------------------*/
uint8_t report(uint8_t cstack)
{

	char outbuf[BUFSIZE+10], version[11], isotime[21];
	const char format_BTM[] = "$S%dBTM,%s,%s";
	const char format_ENV[]="$S%dENV,%3.1fC,%1.0f%%,%3.1fC,%1.0f%%,%3.1fC,%1.0f%%,%3.1fC,%s";
	const char format_TIM[]="$S%dTIM,%s,%s";
	const char format_VAC[]="$S%dVAC,%5.2f,rvac,%5.2f,bvac,%s";
	const char format_VER[] = "$S%dVER,%s,%s";
	float t0, t1, t2, t3, h0, h1, h2;		// temperature and humidity
	float redvac, bluvac;					// red and blue vacuum

	switch(pcmd[cstack].cobject) {

		case 'B':					// Boot time
			get_BOOTTIME(isotime);
			sprintf(outbuf, format_BTM, get_specID(), isotime, pcmd[cstack].cid);
			checksum_NMEA(outbuf);
			send_USART(0, (uint8_t*) outbuf, strlen(outbuf));
			break;

		case 'e':					// Environment (temperature & humidity)
			t0 = get_temperature(0);
			h0 = get_humidity(0);
			t1 = get_temperature(1);
			h1 = get_humidity(1);
			t2 = get_temperature(2);
			h2 = get_humidity(2);
			t3 = get_temperature(3);
			sprintf(outbuf, format_ENV, get_specID(), t0, h0, t1, h1, t2, h2, t3, pcmd[cstack].cid);
			checksum_NMEA(outbuf);
			send_USART(0, (uint8_t*) outbuf, strlen(outbuf));
			break;


		case 't':					// Report current time on specMech clock
			get_time(isotime);
			sprintf(outbuf, format_TIM, get_specID(), isotime, pcmd[cstack].cid);
			checksum_NMEA(outbuf);
			send_USART(0, (uint8_t*) outbuf, strlen(outbuf));
			break;

		case 'v':
			redvac = read_ionpump(REDPUMP);
			bluvac = read_ionpump(BLUEPUMP);
			sprintf(outbuf, format_VAC, get_specID(), redvac, bluvac, pcmd[cstack].cid);
			checksum_NMEA(outbuf);
			send_USART(0, (uint8_t*) outbuf, strlen(outbuf));
			break;

		case 'V':
			get_VERSION(version);	// Send the specMech version
			sprintf(outbuf, format_VER, get_specID(), version, pcmd[cstack].cid);
			checksum_NMEA(outbuf);
			send_USART(0, (uint8_t*) outbuf, strlen(outbuf));
			break;

		default:
			return(ERRORPROMPT);
			break;
	}

	return(GREATERPROMPT);

}

/*------------------------------------------------------------------------------
void send_prompt(uint8_t prompt_flag)
	Puts a command line prompt on the output line. These could be the normal
	greater than (>) for a successful transaction, or an NMEA error line
	followed by the greater than (>).

	If specMech is rebooted, it will only prompt with a single exclamation
	point (!) until it receives an acknowledgment in the form of a single !.
------------------------------------------------------------------------------*/
void send_prompt(uint8_t prompt_flag)
{

	const char str0[] = ">";
	const char str1[] = "!";

	char prompt_str[25];

	switch (prompt_flag) {
		case GREATERPROMPT:
			strcpy(prompt_str, str0);
			send_USART(0, (uint8_t*) prompt_str, strlen(prompt_str));
			break;

		case ERRORPROMPT:
			format_ERR(prompt_str);
			send_USART(0, (uint8_t*) prompt_str, strlen(prompt_str));
			strcpy(prompt_str, str0);
			send_USART(0, (uint8_t*) prompt_str, strlen(prompt_str));
			break;

		case EXCLAIMPROMPT:
			strcpy(prompt_str, str1);
			send_USART(0, (uint8_t*) prompt_str, strlen(prompt_str));
			break;

		default:
			strcpy(prompt_str, str1);
			send_USART(0, (uint8_t*) prompt_str, strlen(prompt_str));
			break;

	}

}

/*------------------------------------------------------------------------------
uint8_t set (char *ptr)
	Sets hardware parameters.

	Input:
		*ptr - Command line pointer that is incremented to find the item to set.

	Returns:
		0 - on success
		1 - invalid item to set (an unrecognized character on the command line).
------------------------------------------------------------------------------*/
uint8_t set(uint8_t cstack)
{

	char object;

	object = pcmd[cstack].cobject;

	switch(object) {
		case 't':
			if (strlen(pcmd[cstack].cvalue) != 19) {
				return(ERRORPROMPT);
			}
			put_time(pcmd[cstack].cvalue);
			break;

		default:
			return(ERRORPROMPT);
	}
	return(GREATERPROMPT);
}
