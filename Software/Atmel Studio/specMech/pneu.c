/*------------------------------------------------------------------------------
pneu.c
	Pneumatic valves and their sensors
------------------------------------------------------------------------------*/

#ifndef PNEUMATICSC
#define PNEUMATICSC

// MCP23008 addresses
//#define HIGHCURRENT	(0x48)	// High current driver for pneumatic valves
#define HIGHCURRENT		(0x40)	// test device is at 0x40

// Valve actions
#define SHUTTERBM		(0x22)	// OR existing value with this first, then
#define SHUTTEROPEN		(0xCE)	// AND with current port pattern to open
#define SHUTTERCLOSE	(0xEC)	// AND with current port pattern to close
#define LEFTBM			(0x44)	// OR existing value with this, then
#define LEFTOPEN		(0xAE)	// All used bits high except left open
#define LEFTCLOSE		(0xEA)	// All used bits high except left close
#define RIGHTBM			(0x88)
#define RIGHTOPEN		(0x6E)
#define RIGHTCLOSE		(0xE6)

#include "mcp23008.c"

// Function prototypes
uint8_t init_pneu(void);
uint8_t set_valves(uint8_t, uint8_t);

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
