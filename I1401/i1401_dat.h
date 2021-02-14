
/* ASCII to BCD conversion */

const char ascii_to_bcd[128] = {
	000, 000, 000, 000, 000, 000, 000, 000,		/* 000 - 037 */
	000, 000, 000, 000, 000, 000, 000, 000,
	000, 000, 000, 000, 000, 000, 000, 000,
	000, 000, 000, 000, 000, 000, 000, 000,
	000, 052, 077, 013, 053, 034, 060, 032,		/* 040 - 077 */
	017, 074, 054, 037, 033, 040, 073, 021,
	012, 001, 002, 003, 004, 005, 006, 007,
	010, 011, 015, 056, 076, 035, 016, 072,
	014, 061, 062, 063, 064, 065, 066, 067,		/* 100 - 137 */
	070, 071, 041, 042, 043, 044, 045, 046,
	047, 050, 051, 022, 023, 024, 025, 026,
	027, 030, 031, 075, 036, 055, 020, 057,
	000, 061, 062, 063, 064, 065, 066, 067,		/* 140 - 177 */
	070, 071, 041, 042, 043, 044, 045, 046,
	047, 050, 051, 022, 023, 024, 025, 026,
	027, 030, 031, 000, 000, 000, 000, 000 };

/* BCD to ASCII conversion - also the "full" print chain */

char bcd_to_ascii[64] = {
	' ', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', '0', '#', '@', ':', '>', '(',
	'^', '/', 'S', 'T', 'U', 'V', 'W', 'X',
	'Y', 'Z', '\'', ',', '%', '=', '\\', '+',
	'-', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
	'Q', 'R', '!', '$', '*', ']', ';', '_',
	'&', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
	'H', 'I', '?', '.', ')', '[', '<', '"' };
