/*------------------------------------------------------------------------------
pneu.c
	Pneumatic valves and their sensors
------------------------------------------------------------------------------*/

#ifndef PNEUMATICSC
#define PNEUMATICSC

// MCP23008 addresses
#define PNEUSENSORS		(0x42)	// GMR sensors on pneumatic cylinders
#define HIGHCURRENT		(0x48)	// High current driver for pneumatic valves
//#define HIGHCURRENT		(0x40)	// test device is at 0x40

// Valve actions
#define SHUTTERBM		(0x22)	// OR existing value with this first, then
#define SHUTTEROPEN		(0xCE)	// AND with this pattern to open
#define SHUTTERCLOSE	(0xEC)	// AND with this pattern to close

#define LEFTBM			(0x44)	// OR existing value with this, then
#define LEFTOPEN		(0xAE)	// AND with this pattern to open
#define LEFTCLOSE		(0xEA)	// AND with this pattern to close

#define RIGHTBM			(0x88)	// OR existing value with this, then
#define RIGHTOPEN		(0x6E)	// AND with this pattern to open
#define RIGHTCLOSE		(0xE6)	// AND with this pattern to close

#include "mcp23008.c"

// Function prototypes
void read_pneusensors(char*, char*, char*, char*);
//char read_shutterstate();
uint8_t init_pneu(void);
uint8_t close_pneu(char);
uint8_t open_pneu(char);
uint8_t set_valves(uint8_t, uint8_t);

//NEED TO FIX ERROR RETURN SITUATION
void read_pneusensors(char *shutter, char *left, char *right, char *air)
{

	uint8_t sensors, state;

	read_MCP23008(PNEUSENSORS, GPIO, &sensors);

	// Shutter
	state = sensors >> 6;
	state &= 0b00000011;
	if (state == 1) {
		*shutter = 'c';
	} else if (state == 2) {
		*shutter = 'o';
	} else if (state == 3) {
		*shutter = 't';
	} else {
		*shutter = 'x';
	}

	// Right
	state = sensors >> 2;
	state &= 0b00000011;
	if (state == 1) {
		*right = 'c';
		} else if (state == 2) {
		*right = 'o';
		} else if (state == 3) {
		*right = 't';
		} else {
		*right = 'x';
	}

	// Left
	state = sensors >> 4;
	state &= 0b00000011;
	if (state == 1) {
		*left = 'o';
	} else if (state == 2) {
		*left = 'c';
	} else if (state == 3) {
		*left = 't';
	} else {
		*left = 'x';
	}

	// Air
	if (sensors & 0b00000010) {
		*air = '0';
	} else {
		*air = '1';
	}
}

/*------------------------------------------------------------------------------
uint8_t init_PNEU(void)
	Initializes the MCP23008 port expander connected to the high current
	driver. The MCP23008 port expander connected to the GMR (pneumatic) sensors
	is assumed to be in input mode.
------------------------------------------------------------------------------*/
uint8_t init_PNEU(void)
{

	uint8_t retval;
	
	if ((retval = write_MCP23008(HIGHCURRENT, IODIR, 0x00))) {
		return(retval);
	}
	if ((retval = write_MCP23008(HIGHCURRENT, OLAT, 0x00))) {
		return(retval);
	}
	return(0);

}

/*------------------------------------------------------------------------------
uint8_t close_pneu(char mech)
	Close the shutter or Hartmann doors

	Pneumatic cylinders move the shutter and Hartmann doors. Each cylinder is
	controlled by a pair of air valves, both of which must be commanded to
	move the shutter or door. One of the pair must be open and the other closed
	to make the item move.

	The valves are electronically activated via an MCP23008 port expander that
	is controlled by the set_valves routine in pneu.c.

	Input:
		mech - a character that selects the shutter, left Hartmann door,
		right Hartmann door, or both doors.
------------------------------------------------------------------------------*/
uint8_t close_pneu(char mech)
{

	switch (mech) {

		case 'b':
			set_valves(LEFTBM, LEFTCLOSE);
			set_valves(RIGHTBM, RIGHTCLOSE);
			break;

		case 'l':
			set_valves(LEFTBM, LEFTCLOSE);
			break;
			
		case 'r':
			set_valves(RIGHTBM, RIGHTCLOSE);
			break;

		case 's':										// Close shutter
			set_valves(SHUTTERBM, SHUTTERCLOSE);
			break;

		default:
			return(ERRORPROMPT);
			break;

	}

	return(GREATERPROMPT);

}

/*------------------------------------------------------------------------------
uint8_t open_pneu(char mechanism)
	Open the shutter or Hartmann doors

	Pneumatic cylinders move the shutter and Hartmann doors. Each cylinder is
	controlled by a pair of air valves, both of which must be commanded to
	move the shutter or door. One of the pair must be open and the other closed
	to make the item move.

	The valves are electronically activated via an MCP23008 port expander that
	is controlled by the set_valves routine in pneu.c.

	Input:
		mechanism - a character that selects the shutter, left Hartmann door,
		right Hartmann door, or both doors.
------------------------------------------------------------------------------*/
uint8_t open_pneu(char mechanism)
{

	switch (mechanism) {

		case 'b':
			set_valves(LEFTBM, LEFTOPEN);
			set_valves(RIGHTBM, RIGHTOPEN);
			break;

		case 'l':
			set_valves(LEFTBM, LEFTOPEN);
			break;
		
		case 'r':
			set_valves(RIGHTBM, RIGHTOPEN);
			break;

		case 's':
			set_valves(SHUTTERBM, SHUTTEROPEN);
			break;

		default:
			return(ERRORPROMPT);

	}

	return(GREATERPROMPT);

}

/*------------------------------------------------------------------------------
set_valves.c
	Set the Clippard valves.
	Read the current valve state, AND that value with the new pattern, then
	write the new valve state.
------------------------------------------------------------------------------*/
uint8_t set_valves(uint8_t bitmap, uint8_t action)
{

	uint8_t retval, old_state, new_state;

	if ((retval = read_MCP23008(HIGHCURRENT, GPIO, &old_state))) {
		return(retval);
	}

	new_state = ((old_state | bitmap) & action);

	if ((retval = write_MCP23008(HIGHCURRENT, OLAT, new_state))) {
		return(retval);
	}

	return(0);

}

#endif
