#ifndef DEFINES_H_
#define DEFINES_H_

// Playfield size (in tiles)
#define BOARD_WIDTH			10
#define BOARD_HEIGHT		15

// Width of a Block in px
#define BLOCK_WIDTH			12
#define BLOCK_HEIGHT		12

// Define board positions for single player game
#define BOARD_TOP 			(16)
#define BOARD_LEFT			(92)

#define NEXT_BLOCK_TOP		(159)
#define NEXT_BLOCK_LEFT		(27)

// Define board positions for 2 player game
// Define top left corner of Player 1 Board
#define PLAYER_1_BOARD_TOP	(9)
#define PLAYER_1_BOARD_LEFT	(4)

// Define top left corner of Player 2 board
#define PLAYER_2_BOARD_TOP	(9)
#define PLAYER_2_BOARD_LEFT	(178)

// Define Next Block Positions
#define PLAYER_1_NEXT_BLOCK_LEFT 	(128)
#define PLAYER_1_NEXT_BLOCK_TOP		(22)

#define PLAYER_2_NEXT_BLOCK_LEFT	(128)
#define PLAYER_2_NEXT_BLOCK_TOP		(110)

// Define Special Block Inventory Positions
#define PLAYER_1_INVENTORY_LEFT		(5)
#define PLAYER_1_INVENTORY_TOP		(202)

#define PLAYER_2_INVENTORY_LEFT		(179)
#define PLAYER_2_INVENTORY_TOP		(202)

// Define 1player Score Positions
#define SCORE_LEFT					(0)
#define SCORE_TOP					(10)

// Define 2player Score Positions
#define PLAYER_1_SCORE_LEFT			(0)
#define PLAYER_1_SCORE_TOP			(26)

#define PLAYER_2_SCORE_LEFT			(22)
#define PLAYER_2_SCORE_TOP			(26)

// Define Events
#define EVENT_NONE			(0)
#define EVENT_END_GAME		(0x01)
#define EVENT_PLAYER1_WIN	(0x02)
#define EVENT_PLAYER2_WIN	(0x04)

#endif // DEFINES_H_
