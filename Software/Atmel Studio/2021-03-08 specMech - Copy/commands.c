#include "globals.h"
#include "wdt.h"
#include "usart.h"
#include "set.h"
#include "specID.h"
#include "nmea.h"
#include "roboclaw.h"
#include "oled.h"
#include "pneu.h"
#include "rtc.h"
#include "report.h"
#include "testroutine.h"
#include "commands.h"

/*------------------------------------------------------------------------------
void commands(void)
	Command loop
------------------------------------------------------------------------------*/
void commands(void)
{

	char cmdline[BUFSIZE];			// BUFSIZE is the size of the ring buffer
	char verb, object;				// Get rid of these and use cstack instead
	uint8_t prompt_flag = GREATERPROMPT;
	static uint8_t cstack = 0;		// Index for pcmd

/*
	// Copy the command string to cmdline
	for (i = 0; recv0_buf.length; i++) {
		cmdline[i] = recv0_buf.data[recv0_buf.tail];
		recv0_buf.length--;
		recv0_buf.tail = (recv0_buf.tail + 1) % BUFSIZE;
		if (cmdline[i] == '\0') {
			break;
		}
	}
	cmdline[i] = '\0';		// Overflow protection
*/
	get_cmdline(cmdline);

	if (!rebootACKd(cmdline)) {
//		send_prompt(EXCLAIMPROMPT);
//		reboot();
		return;
	}
/*
	if (rebootnack) {
		if ((cmdline[0] == '!') && (cmdline[1] == '\0')) {
			init_RTC(511);		// 1-sec RTC clock ticks
			timeoutOLED = 5;	// 5-sec display timeout (min)
			send_prompt(GREATERPROMPT);
			rebootnack = 0;
			return;
		} else if ((cmdline[0] == '!') && (cmdline[1] != '\0')) {
			send_prompt(EXCLAIMPROMPT);
			reboot();
			return;
		} else {
			send_prompt(EXCLAIMPROMPT);
			return;
		}
	}
*/
	// Echo the command back to the user
		echo_cmd(cmdline);

	if (strlen(cmdline) == 0) {		// Catch a terminal cr (not an error)
		send_prompt(GREATERPROMPT);
		return;
	}

	parse_cmd(cmdline, cstack);
	verb = pcmd[cstack].cverb;
	object = pcmd[cstack].cobject;

	switch (verb) {
		case 'c':				// close
			prompt_flag = close_PNEU(object);
			break;

		case 'o':				// open
			prompt_flag = open_PNEU(object);
			break;

		case 'm':				// move
			ROBOMove(cstack);
			break;

		case 'r':				// Report
			prompt_flag = report(cstack);
			break;

		case 's':				// Set
			prompt_flag = set(cstack);
			break;

		case 't':
			testroutine();
			break;

		case 'R':				// Reboot
			send_prompt(GREATERPROMPT);
			_delay_ms(100);		// Avoid finishing the command loop before reboot
			reboot();
			return;

		default:
			prompt_flag = GREATERPROMPT;
			break;			
	}

	cstack = (cstack + 1) % CSTACKSIZE;
	send_prompt(prompt_flag);

}

/*------------------------------------------------------------------------------
void echo_cmd(char *cmdline)
	Echo the command line back to the user, adding the NMEA header and checksum.
------------------------------------------------------------------------------*/
void echo_cmd(char *cmdline)
{

	char format_CMD[] = "$S%dCMD,%s";
	char strbuf[BUFSIZE+10];		// Add $SXCMD, and *HH checksum

		// Format and echo the command line
	sprintf(strbuf, format_CMD, get_SPECID, cmdline);
	checksum_NMEA(strbuf);
	send_USART(0, (uint8_t*) strbuf, strlen(strbuf));

}

/*------------------------------------------------------------------------------
uint8_t get_cmdline(char *cmdline)
	Copies the USART input buffer to the "cmdline" string
------------------------------------------------------------------------------*/
void get_cmdline(char *cmdline)
{

	uint8_t i;

	for (i = 0; recv0_buf.length; i++) {
		cmdline[i] = recv0_buf.data[recv0_buf.tail];
		recv0_buf.length--;
		recv0_buf.tail = (recv0_buf.tail + 1) % BUFSIZE;
		if (cmdline[i] == '\0') {
			break;
		}
	}

	cmdline[i] = '\0';		// Overflow protection

}

/*------------------------------------------------------------------------------
uint8_t isadigit(char d)
	Checks if the character is in the range 0-9
------------------------------------------------------------------------------*/
uint8_t isadigit(char d)
{
	if ((d >= '0') && (d <= '9')) {
		return(1);
	}
	return(0);
}

/*------------------------------------------------------------------------------
uint8_t isaletter(char c)
	Checks if the character is in the range A-Z and a-z
------------------------------------------------------------------------------*/
uint8_t isaletter(char c)
{

	if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
		return(1);
	}
	return(0);

}

/*------------------------------------------------------------------------------
void parse_cmd(char *ptr, uint8_t n)
	Breaks up the command line into its components. The components are:
		verb - The command verb, a single character.
		object - The commanded object, a single character.
		value - The object's new value, a character string.
		ID - An identifier selected by the user, a character string.
	These components are in the ParsedCMD structure defined in main.c. The pcmd
	variable is a ParsedCMD array.
------------------------------------------------------------------------------*/
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

/*------------------------------------------------------------------------------
uint8_t rebootACKd(char *cmdline)
	Checks to see if a processor reboot has been acknowledged with a "!\r"
	command string.

	Input:
		cmdline - the command line string from USART0

	Returns:
		YES if the reboot acknowledge string was received
		NO if it wasn't
------------------------------------------------------------------------------*/
uint8_t rebootACKd(char *cmdline)
{

	static uint8_t rebootnack = 1;
	
	if (rebootnack) {
		if ((cmdline[0] == '!') && (cmdline[1] == '\0')) {
			init_RTC(511);		// 1-sec RTC clock ticks
			timeoutOLED = 5;	// 5-sec display timeout (minimum)
			send_prompt(GREATERPROMPT);
			rebootnack = 0;
			return(YES);
		} else if ((cmdline[0] == '!') && (cmdline[1] != '\0')) {
//			send_prompt(EXCLAIMPROMPT);
			reboot();
			return(NO);
		} else {
			send_prompt(EXCLAIMPROMPT);
			return(NO);
		}
	}

	return(YES);

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
/*
		case ERRORPROMPT:
			printError(prompt_str);
			send_USART(0, (uint8_t*) prompt_str, strlen(prompt_str));
			strcpy(prompt_str, str0);
			send_USART(0, (uint8_t*) prompt_str, strlen(prompt_str));
			break;
*/
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
