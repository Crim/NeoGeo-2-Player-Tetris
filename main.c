/* $Id: main.c,v 1.4 2001/07/18 14:45:58 fma Exp $ */

#include <stdlib.h>
#include <video.h>
#include <input.h>
#include <task.h>
#include "defines.h"
#include "sound.h"

// Pull in TileMaps
extern PALETTE	palettes[];
extern PTILEMAP	onePlayerBg[];
extern PTILEMAP	twoPlayerBg[];
extern PTILEMAP	blocks12[];
extern PTILEMAP	blocks16[];
extern PTILEMAP menuscreen[];
extern PTILEMAP arrow[];


typedef enum
{
	BLOCK_NONE, BLOCK_I, BLOCK_J, BLOCK_L, BLOCK_O, BLOCK_S, BLOCK_T, BLOCK_Z, NUMBER_OF_BLOCKS
} BlockType;


// First line are colours,
// Second line are special blocks, see http://en.wikipedia.org/wiki/TetriNET or notes below
typedef enum
{
	TRANSPARENT, TEAL, PURPLE, BLUE, ORANGE, GREEN, YELLOW, PURPLE_AGAIN,
	ADD_LINE, CLEAR_LINE, CLEAR_SPECIALS, RANDOM_CLEAR, BLOCK_BOMB, BLOCK_QUAKE, BLOCK_GRAVITY, SWITCH_BOARDS, NUKE_FIELD,
	NUMBER_OF_COLOURS

} BlockColour;

typedef enum
{
	DIFFICULTY_EASY, DIFFICULTY_MEDIUM, DIFFICULTY_MVS, DIFFICULTY_HARD, DIFFICULTY_IM_A_TETRIS_MACHINE
} DifficultyLevel;

const int SPRITE_BACKGROUND 	= 1;
const int SPRITE_BLOCK 			= 20;
const int SPRITE_NEXTBLOCK		= 28;
const int SPRITE_INVENTORIES    = 36;
const int SPRITE_BOARD			= 48;

typedef enum
{
	PLAYER_ONE, PLAYER_TWO
} PlayerID;

typedef struct
{
	int x, y;								// X & Y Block Position
	BlockColour colour;						// Colour of Tiles that make up
	BlockType type;							// What type of block are we?
	WORD data;								// Bits flipped to represent this block
} BlockStats;

typedef struct
{
	DifficultyLevel difficulty;				// How well 'ard are we?
	int speed;								// How many frames between moving the blocks?
	int numPlayers;							// How many players
	int event;								// Game Events
}
GameData;

typedef struct
{
	PlayerID ID;									// Which player are we?
	int score;										// Player Score
	int scoreX, scoreY;								// Where to draw the score.
	int frameDelay;									// How long til we move the current block down
	BlockColour board[BOARD_WIDTH][BOARD_HEIGHT];	// Current Play Board
	BlockStats currentBlock;						// Current Falling Block
	BlockStats nextBlock;							// Next Block
	BlockColour specialBlocks[6];					// Array of Special Blocks
	DWORD controllerPort;
	TILEMAP boardTileMap[ BOARD_WIDTH ];			// The board tile map.
	DWORD lastInput;								// What was pressed last tick.
	int dropFrames;									// Frames till the next drop command is executed.
	int blockCount;									// How many blocks have we dropped so far?
	int speed;										// Our actual speed for this player.
}
PlayerData;

const int BlockData[ NUMBER_OF_BLOCKS+1 ] = {0x0000, 0xF, 0xE8, 0xE2, 0xCC, 0xC6, 0xE4, 0x6C, 0xFFFF};
const BlockColour BlockColours[ NUMBER_OF_COLOURS ] =	{TRANSPARENT, TEAL, PURPLE, BLUE, ORANGE, GREEN, YELLOW, PURPLE,	// Standard Colored Blocks
													 	 CLEAR_LINE, BLOCK_GRAVITY, NUKE_FIELD,  SWITCH_BOARDS,				// Special Blocks, These are good, used on your own board
													 	 CLEAR_SPECIALS, RANDOM_CLEAR, BLOCK_BOMB, ADD_LINE, BLOCK_QUAKE };	// These are bad, used on opponent

// Global Game Data
GameData gGameData;

// Player 1 and 2 Data
PlayerData gPlayer1Data;
PlayerData gPlayer2Data;

// Define Functions
void addRowsToBoard(PlayerData *player, int num);
void drawBoard( PlayerData *playerData, int x_offset, int y_offset);
void drawFallingBlock( WORD tBlockGraphic, int board_x, int board_y, int x_offset, int y_offset, BlockColour colour );
void gameInit();
void gameLoop1Player(PlayerData *player);
void gameLoop2Player(PlayerData *player1, PlayerData *player2);
BlockStats generateBlock();
void initBoard(PlayerData *player, BlockColour board[BOARD_WIDTH][BOARD_HEIGHT]);
void pollControls(PlayerData *player);
void resetFrameDelay(PlayerData *player);
void rotateCurrentBlockCW(PlayerData *player);
void setBlockAsPartOfWall( PlayerData *playerData, BlockStats *block, BlockColour board[BOARD_WIDTH][BOARD_HEIGHT] );
int checkCollision(PlayerData *player, int dx, int dy);
void moveBlock(PlayerData *player, int x, int y);
void dropBlock(PlayerData *player);
void clearFullRows(PlayerData *player);
void clearRow(PlayerData *player, int rowNum);
void setGameEvent(WORD event);
void clearGameEvents();
void displayMenu();
void DisplayScore( PlayerData *player1, PlayerData *player2 );
void InitPlayer( PlayerData *player, PlayerID ID, int NumberOfPlayers );
int generateSpecialBlock(PlayerData *player);
void takeSpecialBlocksFromRow(PlayerData *player, int rowNum);
void useCurrentSpecialBlock(PlayerData *player);
void dropCurrentSpecialBlock(PlayerData *player);
void drawSpecialBlockInventories();
void removeFirstBlockFromInventory(PlayerData *player);
PlayerData *getOppositePlayer(PlayerID id);
void clearSpecialBlocksFromBoard(PlayerData *player);
void randomlyClearBlocksFromBoard(PlayerData *player);
void swapPlayerBoards(PlayerData *player1, PlayerData *player2);
void nukePlayerBoard(PlayerData *player);
void gravityPlayerBoard(PlayerData *player);
void shiftBlocksOnBoard(PlayerData *player);
void explodeBombsOnBoard(PlayerData *player);
void replaceBlockOnBoard(PlayerData *player, int pos_x, int pos_y, BlockColour replacement);

/*
	Short Description of how 2 player mode should work

	- I'm thinking that the speed should be constant in 2 player mode (based on difficulty level selected). It seems like increasing block speed
	  based on number of blocks cleared this would only hurt players who are able to play more quickly then their opponent.
	- Special blocks are only used in 2 player games, Single player to remain 'classic' tetris clone (unless anyone has some good suggestions!)
	- Clearing 2+ lines from your own board, adds NumberOfLinesCleared-1 to the other players board, from the bottom up.  Each added line has
	  random blocks missing to prevent it from being a complete line.  IE, I clear 3 lines from my board, you get +2 lines added to the bottom.
	- You cannot have more then 7 special blocks on your *board* at any one time.
	- Special blocks replace normal blocks on your board (IE, instead of board[x][y] = 1-7 for standard coloured tiles, it would become 8-16)
	- Clearing a line with 1 or more special blocks, the special blocks drop down into your 'special block inventory'
	- You can only hold up to a max of 6 special blocks, all further special blocks gained by clearing lines are ignored
	- "Good" Special blocks are used on your own board, "Bad" special blocks are used on opponent, the player does not have the option to
	  use "Good" blocks on an opponent as they would in real PC version of tetrinet.
	- Special blocks must be used in FIFO order.  User has option to "throw away" next block in their inventory/queue

	The following numbers are purely made up, are are open for suggestions/modification as needed:
	- Special blocks cannot appear until after atleast 5 lines have been cleared by a player.
	- Clearing 1 line gives you a 1-10 chance of a single special block appearing
	- Clearing 2 lines gives you a 2-10 chance of 1-2 special blocks appearing
	- Clearing 3 lines gives you a 3-10 chance of 1-2 special blocks eppearing
	- Clearing 4 lines gives you a 4-10 chance of 1-3 special blocks eppearing
	- Clearing 5+ lines gives you a 5-10 chance of 1-4 special blocks eppearing

	From Tetrinet Wikipedia (http://en.wikipedia.org/wiki/TetriNET):
	TetriNET plays like standard multiplayer Tetris but with a twist: clearing rows will cause special blocks to appear in the player's field.
	If a line containing a special block is cleared, then that special block is added to the player's inventory. Clearing multiple lines at once
	increases the number of special blocks received. At any point, a player may use the special block at the beginning of their inventory, the
	effect depends on the type of special block.

	See wikipedia article for def. of special blocks and what they do

	General TODO in no particular order:

	- Music/Sound effects?
	- Figure out how to save high scores?
	- Fix rotateCW function, it rotates around 4x4 matrix, very annoying, 3x3 tetrix pieces should be rotated within 3x3 block matrix, not 4x4
	- Find someone to make k-rad graphics, or SP can fix his not so awesome graphics
	- Add winner/loser graphic when game ends in 2 player mode?
*/

int	main(void)
{
	// Set Palette
	setpalette(0, 11, (const PPALETTE)&palettes);

	while (1)
	{
		// Display Game Menu
		// TODO - Return number of players, return difficulty level
		displayMenu();


		// Init Game
		gameInit();

		// Play Game Start Sound effect
		send_sound_command(SOUND_ADPCM_GAME_START);

		if (gGameData.numPlayers == 1) {
			while(!(gGameData.event & EVENT_END_GAME)) {
				// Run  1 player Game
				gameLoop1Player(&gPlayer1Data);
				wait_vbl();
			}
		}
		else
		{
			while(!(gGameData.event & EVENT_END_GAME)) {
				// Run 2 player Game
				gameLoop2Player(&gPlayer1Data, &gPlayer2Data);
				wait_vbl();
			}
		}
	}

	return 0;
}

void InitPlayer( PlayerData *player, PlayerID ID, int numberOfPlayers )
{
	int x;

	player->score = 0;
	player->ID = ID;
	player->frameDelay = 0;
	player->controllerPort = ID == PLAYER_ONE ? PORT1 : PORT2;
	player->dropFrames = 0;

	// Reset Special Block Inventory
	for (x=0; x<6; x++)
	{
		player->specialBlocks[x] = 0;
	}

	if ( numberOfPlayers == 2 )
	{
		player->scoreY = PLAYER_1_SCORE_TOP;
		if ( ID == PLAYER_ONE )
			player->scoreX = PLAYER_1_SCORE_LEFT;
		else
			player->scoreX = PLAYER_2_SCORE_LEFT;
	}
	else
	{
		player->scoreX = SCORE_LEFT;
		player->scoreY = SCORE_TOP;
	}

	player->blockCount =0;
	player->speed = gGameData.speed;	// As a default starting level.
	memset( &player->boardTileMap, 0, BOARD_WIDTH * BOARD_HEIGHT * sizeof( TILE ) );

	// Init Player 1 Board
	initBoard( player, player->board );

	// Generate Next Block
	player->nextBlock = generateBlock();
}

void gameInit()
{
	// Clear Game events
	clearGameEvents();

	// Set Difficulty Level
	switch (gGameData.difficulty)
	{
	default:
	case DIFFICULTY_EASY:
		gGameData.speed = 50;
		break;
	case DIFFICULTY_MEDIUM:
		gGameData.speed = 30;
		break;
	case DIFFICULTY_MVS:
		gGameData.speed = 20;
		break;
	case DIFFICULTY_HARD:
		gGameData.speed = 10;
		break;
	case DIFFICULTY_IM_A_TETRIS_MACHINE:
		gGameData.speed = 0;
		break;
	}

	InitPlayer( &gPlayer1Data, PLAYER_ONE, gGameData.numPlayers );

	// Conditionally Setup Player 2
	if (gGameData.numPlayers == 2)
		InitPlayer( &gPlayer2Data, PLAYER_TWO, gGameData.numPlayers );

	// Clear All Sprites
	clear_spr();
	set_current_sprite( SPRITE_BACKGROUND );

	// Display Proper Background Image for 1 or 2 Player Game
	if (gGameData.numPlayers == 1)
	{
		// Write 1 player background tilemap
		// 19 sprites, each of 16 pixels width.
		write_sprite_data(0, 0, 15, 240, 32, 19, (const PTILEMAP)&onePlayerBg);

		DisplayScore( &gPlayer1Data, NULL );
	}
	else
	{
		// Write 2 Player Background
		// 19 sprites, each of 16 pixels width.
		write_sprite_data(0, 0, 15, 240, 32, 19, (const PTILEMAP)&twoPlayerBg);

		DisplayScore( &gPlayer1Data, &gPlayer2Data );
	}

}

// The main game loop.
void gameLoop1Player(PlayerData *player)
{
	int tScoreAtStartOfTick = player->score;

	// Do we have an active block?
	if ( player->currentBlock.type != BLOCK_NONE )
	{
		// Reset Current Sprite
		set_current_sprite( SPRITE_BOARD );

		// Draw Board
		drawBoard(&gPlayer1Data, BOARD_LEFT, BOARD_TOP);

		// Subtract drop Delay
		if ( --player->frameDelay < 0 )
		{
			// Attempt to move block down
			moveBlock(player, 0, 1);
		}
		else
		{
			// Poll Joystick & buttons
			pollControls(player);

			// Still falling, so keep drawing.
			set_current_sprite( SPRITE_BLOCK );
			drawFallingBlock(player->currentBlock.data, player->currentBlock.x, player->currentBlock.y, BOARD_LEFT, BOARD_TOP, player->currentBlock.colour );
		}
	}
	else
	{
		// Use nextBlock as currentBlock, generate new nextBlock
		player->currentBlock = player->nextBlock;
		player->nextBlock = generateBlock();

		// Redraw Upcoming Block
		set_current_sprite( SPRITE_NEXTBLOCK );
		drawFallingBlock(player->nextBlock.data, 0, 0, NEXT_BLOCK_LEFT, NEXT_BLOCK_TOP, player->nextBlock.colour);
	}

	if ( player->score != tScoreAtStartOfTick )
	{
		DisplayScore( player, NULL );
	}
}

void gameLoop2Player(PlayerData *player1, PlayerData *player2)
{
	int tScoreAtStartOfTick = player1->score + player2->score;

	// Reset Current Sprite
	set_current_sprite( SPRITE_BOARD );

	// Process First Player
	if ( player1->currentBlock.type != BLOCK_NONE )
	{
		// Draw Board
		drawBoard(&gPlayer1Data, PLAYER_1_BOARD_LEFT, PLAYER_1_BOARD_TOP);

		// Subtract drop Delay
		if ( --player1->frameDelay < 0 )
		{
			// Attempt to move block down
			moveBlock(player1, 0, 1);
		}
		else
		{
			// Poll Joystick & buttons
			pollControls(player1);

			// Still falling, so keep drawing.
			set_current_sprite( SPRITE_BLOCK );
			drawFallingBlock(player1->currentBlock.data, player1->currentBlock.x, player1->currentBlock.y, PLAYER_1_BOARD_LEFT, PLAYER_1_BOARD_TOP, player1->currentBlock.colour );
		}
	}
	else
	{
		player1->currentBlock = player1->nextBlock;
		player1->nextBlock = generateBlock();

		// Redraw Upcoming Block
		set_current_sprite( SPRITE_NEXTBLOCK );
		drawFallingBlock(player1->nextBlock.data, 0, 0, PLAYER_1_NEXT_BLOCK_LEFT, PLAYER_1_NEXT_BLOCK_TOP, player1->nextBlock.colour);
	}

	// Process 2nd Player
	if ( player2->currentBlock.type != BLOCK_NONE )
	{
		set_current_sprite( SPRITE_BOARD + BOARD_WIDTH );

		// Draw Board
		drawBoard(&gPlayer2Data, PLAYER_2_BOARD_LEFT, PLAYER_2_BOARD_TOP);

		// Subtract drop Delay
		if ( --player2->frameDelay < 0 )
		{
			// Attempt to move block down
			moveBlock(player2, 0, 1);
		}
		else
		{
			// Poll Joystick & buttons
			pollControls(player2);

			// Still falling, so keep drawing.
			set_current_sprite( SPRITE_BLOCK + 4 );
			drawFallingBlock(player2->currentBlock.data, player2->currentBlock.x, player2->currentBlock.y, PLAYER_2_BOARD_LEFT, PLAYER_2_BOARD_TOP, player2->currentBlock.colour );
		}
	}
	else
	{
		player2->currentBlock = player2->nextBlock;
		player2->nextBlock = generateBlock();

		// Redraw Upcoming Block
		set_current_sprite( SPRITE_NEXTBLOCK + 4 );
		drawFallingBlock(player2->nextBlock.data, 0, 0, PLAYER_2_NEXT_BLOCK_LEFT, PLAYER_2_NEXT_BLOCK_TOP, player2->nextBlock.colour);
	}

	if ( player1->score + player2->score != tScoreAtStartOfTick )
	{
		DisplayScore( player1, player2 );
	}

}

// Set the block into the wall.
void setBlockAsPartOfWall( PlayerData *playerData, BlockStats *block, BlockColour board[BOARD_WIDTH][BOARD_HEIGHT] )
{
	WORD tBlockGraphic = block->data;
	int row;

	// Set Type to none
	block->type = BLOCK_NONE;

	//  e.g. Sets the top left block to be type 1.
	//	playerData->boardTileMap[ 0 ].tiles[ 0 ].block_number = 0x0152;
	//	playerData->boardTileMap[ 0 ].tiles[ 0 ].attributes = 0x0300;

	for (row = 0 ; row < 4 ; row++ )
	{
		int tColumn;
		int tBit = 0x01;
		for ( tColumn = 0 ; tColumn < 4 ; tColumn++ )
		{
			if ( tBlockGraphic & tBit )
			{
				playerData->boardTileMap[ (block->x + 3 - tColumn) ].tiles[ block->y + row ].block_number = ((PTILE)(&blocks16[ (int)block->colour ]))->block_number;
				playerData->boardTileMap[ (block->x + 3 - tColumn) ].tiles[ block->y + row ].attributes = ((PTILE)(&blocks16[ (int)block->colour ]))->attributes;
				board[block->x + 3 - tColumn][block->y+row] = block->colour;
			}

			tBit <<= 1;
		}

		// Now deal with the next row.
		tBlockGraphic >>= 4;
	}

	playerData->blockCount++;

	if (gGameData.numPlayers == 1)
	{
		if ( playerData->blockCount % 5 == 0 )
		{
			// Move up a gear!
			if ( playerData->speed > 0 )
				playerData->speed--;
		}
	}

	// Play Sound Effect
	send_sound_command(SOUND_ADPCM_BLOCK_HITS_BOTTOM);
}

// Detects collisions against walls and background wall
int checkCollision(PlayerData *player, int dx, int dy)
{
	int newx, newy, i;
	WORD tBlockGraphic = player->currentBlock.data;

	newx = player->currentBlock.x + dx;
	newy = player->currentBlock.y + dy;

	for (i=0; i < 4; i++)
	{
		if ( tBlockGraphic & 0x01 )
		{
			// Check Against left, right, and bottom borders
			if ( (newx+3 < 0) || (newx+3 >= BOARD_WIDTH) || (newy+i == BOARD_HEIGHT))
				return 1;	// Hit

			// Check against Existing Cells
			if (player->board[newx+3][newy+i] != 0)
				return 1;	// Hit
		}
		if ( tBlockGraphic & 0x02 )
		{
			// Check Against left, right, and bottom borders
			if ( (newx+2 < 0) || (newx+2 >= BOARD_WIDTH) || (newy+i == BOARD_HEIGHT))
				return 1;	// Hit

			// Check against Existing Cells
			if (player->board[newx+2][newy+i] != 0)
				return 1;	// Hit
		}
		if ( tBlockGraphic & 0x04 )
		{
			// Check Against left, right, and bottom borders
			if ( (newx+1 < 0) || (newx+1 >= BOARD_WIDTH) || (newy+i == BOARD_HEIGHT))
				return 1;	// Hit

			// Check against Existing Cells
			if (player->board[newx+1][newy+i] != 0)
				return 1;	// Hit
		}
		if ( tBlockGraphic & 0x08 )
		{
			// Check Against left, right, and bottom borders
			if ( (newx < 0) || (newx >= BOARD_WIDTH) || (newy+i == BOARD_HEIGHT))
				return 1;	// Hit

			// Check against Existing Cells
			if (player->board[newx][newy+i] != 0)
				return 1;	// Hit
		}

		// Now deal with the next row.
		tBlockGraphic >>= 4;
	}
	return 0;
}

// Inits board matrix to all 0's
void initBoard( PlayerData *player, BlockColour board[ BOARD_WIDTH ][ BOARD_HEIGHT ] )
{
	int x, y;
	// Initialize Board
	for (x=0; x<BOARD_WIDTH; x++) {
		for (y=0; y<BOARD_HEIGHT; y++) {
			// Init board to blank
			board[x][y] = 0;

			// Clear the board sprites accordingly too.
			player->boardTileMap[ x ].tiles[ y ].block_number = 0x0000;
			player->boardTileMap[ x ].tiles[ y ].attributes = 0x0000;
		}
	}
}

// Draws the board in its current state as defined by the board matrix
void drawBoard( PlayerData *playerData, int x_offset, int y_offset)
{
	// IMG: This is verrrry fast. Just one blit. :)
	// (192-1) height because we need to draw them 3/4 size.
	write_sprite_data( x_offset, y_offset, 11, 191, BOARD_HEIGHT, BOARD_WIDTH , (PTILEMAP)(&playerData->boardTileMap[ 0 ]) );
}

// Draws a single block on the board
void drawFallingBlock( WORD tBlockGraphic, int board_x, int board_y, int x_offset, int y_offset, BlockColour colour )
{
	int row;

	// Start with the LSBs....

	// We have four rows.
	// If we just look at the least significant nibble, we get the first row.
	// If we then shuffle the bits along by 4 we get the next row, and so on.
	for ( row = 0 ; row < 4 ; row++ )
	{
		if ( tBlockGraphic & 0x01 )
			write_sprite_data( (3*BLOCK_WIDTH)+(board_x*BLOCK_WIDTH)+x_offset, (row*BLOCK_HEIGHT)+(board_y*BLOCK_HEIGHT)+y_offset,15,255,1,1,(const PTILEMAP)&blocks12[(int)colour]);
		if ( tBlockGraphic & 0x02 )
			write_sprite_data( (2*BLOCK_WIDTH)+(board_x*BLOCK_WIDTH)+x_offset, (row*BLOCK_HEIGHT)+(board_y*BLOCK_HEIGHT)+y_offset,15,255,1,1,(const PTILEMAP)&blocks12[(int)colour]);
		if ( tBlockGraphic & 0x04 )
			write_sprite_data( (1*BLOCK_WIDTH)+(board_x*BLOCK_WIDTH)+x_offset, (row*BLOCK_HEIGHT)+(board_y*BLOCK_HEIGHT)+y_offset,15,255,1,1,(const PTILEMAP)&blocks12[(int)colour]);
		if ( tBlockGraphic & 0x08 )
			write_sprite_data( (board_x*BLOCK_WIDTH)+x_offset, (row*BLOCK_HEIGHT)+(board_y*BLOCK_HEIGHT)+y_offset,15,255,1,1,(const PTILEMAP)&blocks12[(int)colour]);

		// Now deal with the next row.
		tBlockGraphic >>= 4;
	}
}

// Generates a random tetris piece
BlockStats generateBlock() {
	BlockStats tBlockStats;

	// We can't have a block type zero (empty block).
	tBlockStats.type = (rand() % (NUMBER_OF_BLOCKS-1))+1;
	tBlockStats.data = BlockData[ tBlockStats.type ];
	tBlockStats.colour = BlockColours[ tBlockStats.type ];
	tBlockStats.x = 0;
	tBlockStats.y = 0;

	return tBlockStats;
}

void resetFrameDelay(PlayerData *player)
{
	player->frameDelay = player->speed;
}

// Polls a player's controls
void pollControls(PlayerData *player)
{
	DWORD input;

	// Poll JoyStick, we need to change this to READ_BIOS, but limit the input rate...
	input = poll_joystick(player->controllerPort, READ_BIOS);

	if (input & JOY_DOWN && player->dropFrames == 0) {
		// Attempt to move block down
		moveBlock(player, 0, 1);
		player->dropFrames = 2;
	}
	else if (input & JOY_LEFT && !(player->lastInput & JOY_LEFT)) {
		// Attempt to move block left
		moveBlock(player, -1, 0);
	}
	else if (input & JOY_RIGHT && !(player->lastInput & JOY_RIGHT)) {
		// Attempt to move block right
		moveBlock(player, 1, 0);
	}
	else if (input & JOY_A && !(player->lastInput & JOY_A)) {
		// Button A rotates CW
		rotateCurrentBlockCW(player);
	}
	else if (input & JOY_B && !(player->lastInput & JOY_B)) {
		// Hard Drop
		dropBlock(player);
	}
	else if (input & JOY_C && !(player->lastInput & JOY_C)) {
		// Use Special Block, 2 player mode only
		useCurrentSpecialBlock(player);
	}
	else if (input & JOY_D && !(player->lastInput & JOY_D)) {
		// Drop/Delete Current Special Block, 2 player mode only
		//dropCurrentSpecialBlock(player);

		// Only for debugging/testing:
		generateSpecialBlock(player);
	}

	if ( player->dropFrames )
		player->dropFrames--;

	player->lastInput = input;
}

void moveBlock(PlayerData *player, int x, int y)
{
	if (checkCollision(player, x, y))
	{
		// We collided with something!
		if (y == 1)
		{
			// Move was downwards
			if (player->currentBlock.y < 1)
			{
				// Game is over, we collided in first row
				setGameEvent(EVENT_END_GAME);
			}
			else
			{
				// The falling block has reached the bottom
				// So copy its cells tothe board
				setBlockAsPartOfWall( player, &(player->currentBlock), player->board);

				// Check to see if we have a full row
				clearFullRows(player);
			}
		}
	}
	else
	{
		// We can move the block, nothing is blocking us!
		player->currentBlock.x += x;
		player->currentBlock.y += y;
	}

	// If we moved down, then reset the Fall Timer
	if (y == 1)
		resetFrameDelay(player);

}

// Rotates a block Clockwise,
// This needs to be re-written....
void rotateCurrentBlockCW(PlayerData *player)
{
	WORD rotated = 0x0;
	WORD tmp = 0x0;
	int i;

	// No need to rotate O block
	if (player->currentBlock.type == BLOCK_O)
		return;

	// First row
	if (player->currentBlock.data & 0x01)
		rotated = rotated ^ 0x08;
	if (player->currentBlock.data & 0x02)
		rotated = rotated ^ 0x80;
	if (player->currentBlock.data & 0x04)
		rotated = rotated ^ 0x800;
	if (player->currentBlock.data & 0x08)
		rotated = rotated ^ 0x8000;

	// 2nd row
	if (player->currentBlock.data & 0x10)
		rotated = rotated ^ 0x04;
	if (player->currentBlock.data & 0x20)
		rotated = rotated ^ 0x40;
	if (player->currentBlock.data & 0x40)
		rotated = rotated ^ 0x400;
	if (player->currentBlock.data & 0x80)
		rotated = rotated ^ 0x4000;

	// 3rd Row
	if (player->currentBlock.data & 0x100)
		rotated = rotated ^ 0x02;
	if (player->currentBlock.data & 0x200)
		rotated = rotated ^ 0x20;
	if (player->currentBlock.data & 0x400)
		rotated = rotated ^ 0x200;
	if (player->currentBlock.data & 0x800)
		rotated = rotated ^ 0x2000;

	// 4th row
	if (player->currentBlock.data & 0x1000)
		rotated = rotated ^ 0x01;
	if (player->currentBlock.data & 0x2000)
		rotated = rotated ^ 0x10;
	if (player->currentBlock.data & 0x4000)
		rotated = rotated ^ 0x100;
	if (player->currentBlock.data & 0x8000)
		rotated = rotated ^ 0x1000;

	// Save Copy of rotated block
	tmp = rotated;

	// Verify that there are no collisions now before making it officially rotated
	for (i=0; i < 4; i++)
	{
		if ( rotated & 0x01 )
		{
			// Check Against left, right, and bottom borders
			if ( (player->currentBlock.x + 3 < 0) || (player->currentBlock.x+3 >= BOARD_WIDTH) || (player->currentBlock.y+i >= BOARD_HEIGHT))
				return;	// Hit

			// Check against Existing Cells
			if (player->board[player->currentBlock.x+3][player->currentBlock.y+i] != 0)
				return;	// Hit
		}
		if ( rotated & 0x02 )
		{
			// Check Against left, right, and bottom borders
			if ( (player->currentBlock.x + 2 < 0) || (player->currentBlock.x+2 >= BOARD_WIDTH) || (player->currentBlock.y+i >= BOARD_HEIGHT))
				return;	// Hit

			// Check against Existing Cells
			if (player->board[player->currentBlock.x+2][player->currentBlock.y+i] != 0)
				return;	// Hit
		}
		if ( rotated & 0x04 )
		{
			// Check Against left, right, and bottom borders
			if ( (player->currentBlock.x + 1 < 0) || (player->currentBlock.x+1 >= BOARD_WIDTH) || (player->currentBlock.y+i >= BOARD_HEIGHT))
				return;	// Hit

			// Check against Existing Cells
			if (player->board[player->currentBlock.x+1][player->currentBlock.y+i] != 0)
				return;	// Hit
		}
		if ( rotated & 0x08 )
		{
			// Check Against left, right, and bottom borders
			if ( (player->currentBlock.x < 0) || (player->currentBlock.x >= BOARD_WIDTH) || (player->currentBlock.y+i >= BOARD_HEIGHT))
				return;	// Hit

			// Check against Existing Cells
			if (player->board[player->currentBlock.x][player->currentBlock.y+i] != 0)
				return;	// Hit
		}

		// Now deal with the next row.
		rotated >>= 4;
	}

	// Well if we made it this far, we are safe to rotate!
	player->currentBlock.data = tmp;

	// Play rotate sound
	send_sound_command(SOUND_ADPCM_ROTATE_BLOCK);
}

// Drops a block
void dropBlock(PlayerData *player)
{
	int y=1;

   // Calculate number of cells to drop
	while (!checkCollision(player, 0, y)) {
		y++;
	}
	moveBlock(player, 0, y-1);

	// Add Block To Wall, prevents user from moving block after it falls
	player->frameDelay = 0;

	// Play Drop Block Sound
	send_sound_command(SOUND_ADPCM_BLOCK_DROP);
}

void DisplayScore( PlayerData *player1, PlayerData *player2 )
{
	clear_fix();

	if ( player2 )
	{
		textoutf( player1->scoreX, player1->scoreY + 1, 0, 0, "SCORE 1: %d", player1->score );
		textoutf( player2->scoreX, player1->scoreY + 1, 0, 0, "SCORE 2: %d", player2->score );
	}
	else
	{
		textout( player1->scoreX, player1->scoreY, 0, 0, "SCORE:" );
		textoutf( player1->scoreX, player1->scoreY + 1, 0, 0, "%d", player1->score );
	}
}

void clearFullRows(PlayerData *player)
{
	int i, j, hasFullRow = 0;
	int tRowsCleared = 0;
	PlayerData *opponent;

	for (j = 0; j < BOARD_HEIGHT; j++)
	{
		hasFullRow = 1;
		for (i = 0; i < BOARD_WIDTH; i++)
		{
			if (player->board[i][j] == 0)
			{
				hasFullRow = 0;
			}
		}
		// If we found a full row we need to remove that row from the map
		// we do that by just moving all the above rows one row below
		if (hasFullRow == 1)
		{
			// If 2 player game, first check the row for special blocks
			if (gGameData.numPlayers == 2)
			{
				// Grab special blocks and drop into player inventory first
				takeSpecialBlocksFromRow(player, j);
			}
			// Clear Row
			clearRow(player, j);
			tRowsCleared++;
		}
	}

	// Increment Player Stats/Points
	switch ( tRowsCleared )
	{
	default:
	case 0:
		// No score this tick!
		break;
	case 1:
		// Ooo.. one whole row gone kerblammo.
		player->score += 10;
		send_sound_command(SOUND_ADPCM_CLEAR_ONE_LINE);
		break;
	case 2:
		// Two rows! Not bad...
		player->score += 50;
		send_sound_command(SOUND_ADPCM_CLEAR_ONE_LINE);
		break;
	case 3:
		// So close....
		player->score += 250;
		send_sound_command(SOUND_ADPCM_CLEAR_ONE_LINE);
		break;
	case 4:
		// WOHOO!
		player->score += 2000;
		send_sound_command(SOUND_ADPCM_CLEAR_FOUR_LINES);
		break;
	}

	// If two player game,
	if (gGameData.numPlayers == 2)
	{
		// add rows to opposite Player
		opponent = getOppositePlayer(player->ID);
		addRowsToBoard(opponent, (tRowsCleared-1));

		// Generate Special Blocks
		j = tRowsCleared;
		for (i=0; i<tRowsCleared; i++)
		{
			if (( rand()%10) <= tRowsCleared)
			{
				// Add special Char
				if (generateSpecialBlock(player) == 0)
					return;
			}
		}
	}

}

void clearRow(PlayerData *player, int rowNum)
{
	int x, y;

	for (x = 0; x < BOARD_WIDTH; x++)
	{
		for (y = rowNum; y > 1; y--)
		{
			// Play some animation?
			player->board[x][y] = player->board[x][y - 1];

			// Move the board sprites accordingly too.
			player->boardTileMap[ x ].tiles[ y ].block_number = player->boardTileMap[ x ].tiles[ y-1 ].block_number;
			player->boardTileMap[ x ].tiles[ y ].attributes = player->boardTileMap[ x ].tiles[ y-1 ].attributes;
		}
	}
}

void addRowsToBoard(PlayerData *player, int num)
{
	int x,y;
	int i;
	int hasMissingBlock = 0;
	int hasNormalBlock = 0;

	for (i=0; i<num; i++)
	{
		// Reset Flags
		hasMissingBlock = 0;
		hasNormalBlock = 0;

		// Verify that the current falling block will not collide with the next row
		if (checkCollision(player, 0, 1))
		{
			// Block will collide when we bump up a line
			// Make falling block part of background
			setBlockAsPartOfWall( player, &(player->currentBlock), player->board);
		}

		// Add Row to bottom, pushing all other rows up 1 row
		// Move all rows up 1
		for (x=0; x<BOARD_WIDTH; x++)
		{
			for (y=1; y<BOARD_HEIGHT; y++)
			{
				player->board[x][y-1] = player->board[x][y];

				// Move the board sprites up accordingly too.
				player->boardTileMap[ x ].tiles[ y-1 ].block_number = player->boardTileMap[ x ].tiles[ y ].block_number;
				player->boardTileMap[ x ].tiles[ y-1 ].attributes = player->boardTileMap[ x ].tiles[ y ].attributes;
			}
		}

		// Generate blocks for Row 0
		for (x=0; x<BOARD_WIDTH; x++)
		{
			// Generate random blocks for new bottom row, including 0 for missing blocks
			player->board[x][(BOARD_HEIGHT-1)] = BlockColours[(rand() % (NUMBER_OF_BLOCKS-1))];
			if (player->board[x][(BOARD_HEIGHT-1)] == TRANSPARENT)
			{
				hasMissingBlock = 1;

				// Update boardTileMaps
				player->boardTileMap[ x ].tiles[ (BOARD_HEIGHT-1) ].block_number = 0;
				player->boardTileMap[ x ].tiles[ (BOARD_HEIGHT-1) ].attributes = 0;
				//player->boardTileMap[ x ].tiles[ (BOARD_HEIGHT-1) ] = 0;
			}
			else
			{
				hasNormalBlock = 1;

				// Update boardTileMaps
				player->boardTileMap[ x ].tiles[ (BOARD_HEIGHT-1) ].block_number = ((PTILE)(&blocks16[ (int)player->board[x][(BOARD_HEIGHT-1)] ]))->block_number;
				player->boardTileMap[ x ].tiles[ (BOARD_HEIGHT-1) ].attributes = ((PTILE)(&blocks16[ (int)player->board[x][(BOARD_HEIGHT-1)] ]))->attributes;
			}
		}

		// Ensure we have atleast 1 missing Block
		if (hasMissingBlock == 0)
		{
			player->board[2][(BOARD_HEIGHT-1)] = BlockColours[1];
		}

		// Ensure we have atleast 1 normal Block
		if (hasNormalBlock == 0)
		{
			player->board[2][(BOARD_HEIGHT-1)] = BlockColours[0];
		}
	}
}

int generateSpecialBlock(PlayerData *player)
{
	// Replace random existing blocks with special blocks
	int x, y;
	int foundBlock = 0;		// Flag for if we even have any blocks
	int firstRow = -1;		// First row that has blocks in it
	BlockColour randomSpecial = BlockColours[ ( rand() % 9 + 8 ) ];

	// First find the first row which has a block in it
	for (y=0; y<BOARD_HEIGHT; y++)
	{
		for (x=0; x<BOARD_WIDTH; x++)
		{
			// Exclude special blocks
			if (player->board[x][y] != TRANSPARENT && player->board[x][y] < 8)
			{
				// Set First Row
				firstRow = y;

				// Break out of loops
				x = BOARD_WIDTH;
				y = BOARD_HEIGHT;
			}
		}
	}

	// If firstRow still equals -1, we dont have any blocks, return now
	if (firstRow == -1)
		return(0);

	do
	{
		// Randomly Generate an x and y position
		for (y=(rand() % (BOARD_HEIGHT-firstRow)+firstRow); y<BOARD_HEIGHT; y++)
		{
			for (x=(rand() % BOARD_WIDTH); x<BOARD_WIDTH; x++)
			{
				if (player->board[x][y] != TRANSPARENT && player->board[x][y] < 8)
				{
					// Make this block a special Char
					player->board[x][y] = randomSpecial;

					// Write TileMap Board
					player->boardTileMap[ x ].tiles[ y ].block_number = ((PTILE)(&blocks16[ (int)player->board[x][y] ]))->block_number;
					player->boardTileMap[ x ].tiles[ y ].attributes = ((PTILE)(&blocks16[ (int)player->board[x][y] ]))->attributes;

					// Break out of loop
					x = BOARD_WIDTH;
					y = BOARD_HEIGHT;
					foundBlock = 10;
				}
			}
		}
		foundBlock++;
	}
	while (foundBlock < 10);

	// Return True
	return(1);
}

// Takes Special Chars out of a cleared row, and drops into players inventory
void takeSpecialBlocksFromRow(PlayerData *player, int rowNum)
{
	int x, inventorySpot = -1;

	// Find first available inventory spot
	for (x=0; x<6; x++)
	{
		if (player->specialBlocks[x] == 0)
		{
			inventorySpot = x;
			x = 6;						// Break out of loop
		}
	}

	// Make sure our inventory isnt full already
	if (inventorySpot == -1)
		return;

	// Now Retrieve our Special Blocks
	for (x=0; x<BOARD_WIDTH; x++)
	{
		if ((int)player->board[x][rowNum] >= (int)ADD_LINE)
		{
			// Found a special block!
			player->specialBlocks[inventorySpot] = player->board[x][rowNum];

			// Check if our Inventory is full
			inventorySpot++;
			if (inventorySpot == 6)
				x = BOARD_WIDTH;		// Break out of loop
		}
	}

	// Now Draw Inventory Blocks
	drawSpecialBlockInventories();
}

// use the next special block in players inventory
void useCurrentSpecialBlock(PlayerData *player)
{
	BlockColour currentBlock = player->specialBlocks[0];
	PlayerData *opponent;

	// Make sure we have a special Block at all
	if (currentBlock == 0)
		return;

	// Drop off block from inventory
	removeFirstBlockFromInventory(player);

	// Now execute Special Block
	switch (currentBlock)
	{
		case ADD_LINE:
			// Add a single Line to opponents board
			opponent = getOppositePlayer(player->ID);
			addRowsToBoard(opponent, 1);
			break;
		case CLEAR_LINE:
			// Clear bottom row from players board
			clearRow(player, (BOARD_HEIGHT-1));
			break;
		case CLEAR_SPECIALS:
			// Clear special blocks from opponents board
			opponent = getOppositePlayer(player->ID);
			clearSpecialBlocksFromBoard(opponent);
			break;
		case RANDOM_CLEAR:
			// Clear random blocks from opponents board
			opponent = getOppositePlayer(player->ID);
			randomlyClearBlocksFromBoard(opponent);
			break;
		case BLOCK_BOMB:
			// Explode B special blocks on opponents board
			opponent = getOppositePlayer(player->ID);
			explodeBombsOnBoard(opponent);
			break;
		case BLOCK_QUAKE:
			// Shift bocks on opponents board
			opponent = getOppositePlayer(player->ID);
			shiftBlocksOnBoard(opponent);
			break;
		case BLOCK_GRAVITY:
			// Drops blocks on players board, causing full lines to be removed
			gravityPlayerBoard(player);
			break;
		case SWITCH_BOARDS:
			// Swap boards w/ opponent
			opponent = getOppositePlayer(player->ID);
			swapPlayerBoards(player, opponent);
			break;
		case NUKE_FIELD:
			// Clear all blocks on players board
			nukePlayerBoard(player);
			break;
		default:
			// Do nothing? Unknown special block?
			break;
	}

	// Now Draw Inventory Blocks
	drawSpecialBlockInventories();
}

// Removes all special blocks from players board
// Replaces them with random colored block
void clearSpecialBlocksFromBoard(PlayerData *player)
{
	int x, y;
	// Loop thru board
	for (x=0; x<BOARD_WIDTH; x++)
	{
		for (y=0; y<BOARD_HEIGHT; y++)
		{
			if (player->board[x][y] >= ADD_LINE)
			{
				// Need to replace this block
				replaceBlockOnBoard(player,x,y, (rand() % 7) + 1);
			}
		}
	}
}

void randomlyClearBlocksFromBoard(PlayerData *player)
{
	int x, y;
	// Loop thru board
	for (x=0; x<BOARD_WIDTH; x++)
	{
		for (y=0; y<BOARD_HEIGHT; y++)
		{
			// Decide if we should randomly replace this
			if (player->board[x][y] > 0 && (rand() % 10) == 5)
			{
				// Need to replace this block
				replaceBlockOnBoard(player,x,y, 0);
			}
		}
	}
}

// Swaps boards between the two players
void swapPlayerBoards(PlayerData *player1, PlayerData *player2)
{
	int x, y;
	/*
		Swap the following variables within the PlayerData structs

		int frameDelay;									// How long til we move the current block down
		BlockColour board[BOARD_WIDTH][BOARD_HEIGHT];	// Current Play Board
		BlockStats currentBlock;						// Current Falling Block
		TILEMAP boardTileMap[ BOARD_WIDTH ];			// The board tile map.
	*/
	int tmpFrameDelay;
	BlockColour tmpBoard;
	TILE tmpTile;
	BlockStats tmpCurrentBlock;

	// Swap FrameDelays
	tmpFrameDelay = player1->frameDelay;
	player1->frameDelay = player2->frameDelay;
	player2->frameDelay = tmpFrameDelay;

	// Swap Board Arrays and tilemaps
	for (x=0; x<BOARD_WIDTH; x++)
	{
		for (y=0; y<BOARD_HEIGHT; y++)
		{
			// Swap Board Array
			tmpBoard = player1->board[x][y];
			player1->board[x][y] = player2->board[x][y];
			player2->board[x][y] = tmpBoard;

			// Swap Tilemaps
			tmpTile.block_number = player1->boardTileMap[x].tiles[y].block_number;
			tmpTile.attributes = player1->boardTileMap[x].tiles[y].attributes;

			player1->boardTileMap[x].tiles[y].block_number = player2->boardTileMap[x].tiles[y].block_number;
			player1->boardTileMap[x].tiles[y].attributes = player2->boardTileMap[x].tiles[y].attributes;

			player2->boardTileMap[x].tiles[y].block_number = tmpTile.block_number;
			player2->boardTileMap[x].tiles[y].attributes = tmpTile.attributes;
		}
	}

	// Swap Current Blocks
	tmpCurrentBlock = player1->currentBlock;
	player1->currentBlock = player2->currentBlock;
	player2->currentBlock = tmpCurrentBlock;
}

// Clears out players board
void nukePlayerBoard(PlayerData *player)
{
	int x, y;

	// Swap Board Arrays and tilemaps
	for (x=0; x<BOARD_WIDTH; x++)
	{
		for (y=0; y<BOARD_HEIGHT; y++)
		{
			player->board[x][y] = 0;

			player->boardTileMap[ x ].tiles[ y ].block_number = 0;
			player->boardTileMap[ x ].tiles[ y ].attributes = 0;
		}
	}
}

// Drops blocks down, clears full lines
void gravityPlayerBoard(PlayerData *player)
{
	int x, y;
	// Start from the bottom row, work our way up, if an open space exists below a block, move it down one
	for (y=(BOARD_HEIGHT-2); y>=0; y--)
	{
		for (x=0; x<BOARD_WIDTH; x++)
		{
			if (player->board[x][y+1] == 0 && player->board[x][y] != 0)
			{
				// Found open space below block, pull it down
				replaceBlockOnBoard(player, x, (y+1), player->board[x][y]);

				// Now Replace pulled down block with empty space
				replaceBlockOnBoard(player, x, y, 0);
			}
		}
	}
	// At the end, call clearFullRows()
	clearFullRows(player);
}

// Similar to gravityPlayerBoard, except shift left or right,
void shiftBlocksOnBoard(PlayerData *player)
{
	int x, y, shiftDirection;
	for (y=0; y<BOARD_HEIGHT; y++)
	{
		// Determine shift direction for this show
		shiftDirection = rand() % 2;

		if (shiftDirection == 0)
		{
			// Shift Left
			for (x=1; x<BOARD_WIDTH; x++)
			{
				if (player->board[x-1][y] == 0 && player->board[x][y] > 0)
				{
					// Shift Block to the left
					replaceBlockOnBoard(player, (x-1), y, player->board[x][y]);

					// Now Replace shifted block with empty space
					replaceBlockOnBoard(player, x, y, 0);
				}
			}
		}
		else
		{
			// Shift Right
			for (x=0; x<(BOARD_WIDTH-1); x++)
			{
				if (player->board[x+1][y] == 0 && player->board[x][y] > 0)
				{
					// Shift Block to the right
					replaceBlockOnBoard(player, (x+1), y, player->board[x][y]);

					// Now Replace shifted block with empty space
					replaceBlockOnBoard(player, x, y, 0);
				}
			}
		}
	}
}

// Explode any Bomb Special Blocks on Board
void explodeBombsOnBoard(PlayerData *player)
{
	/*
		Exploding a Bomb block does the following:

		- Replace B block with space,
		- If neighbor block is empty, 50% chance it becomes normal block
		- If neighbor block is not empty, 50% chance it becomes transparent block
		- Call clearFullLines() (Or better, ensure no full lines are created during this, but I'm lazy so for now do this)
	*/
	int x, y, canAbove, canBelow, canLeft, canRight;
	for (y=0; y<BOARD_HEIGHT; y++)
	{
		for (x=0; x<BOARD_WIDTH; x++)
		{
			// Find Bomb Special Block
			if (player->board[x][y] == BLOCK_BOMB)
			{
				// Found a bomb block
				// Find Constraints for 'sploding
				if (y > 0)
					canAbove = 1;
				else
					canAbove = 0;

				if (y < (BOARD_HEIGHT-1))
					canBelow = 1;
				else
					canBelow = 0;

				if (x > 0)
					canLeft = 1;
				else
					canLeft = 0;

				if (x < (BOARD_WIDTH-1))
					canRight = 1;
				else
					canRight = 0;

				// Now that we know what we can explode, lets splode dis bizzle, some uglyness coming up

				// Top left
				if (canAbove && canLeft && (rand()%2) == 0)
				{
					if (player->board[x-1][y-1] == 0)
						replaceBlockOnBoard(player,(x-1),(y-1),(rand()%8));	// Replace random Block
					else
						replaceBlockOnBoard(player,(x-1),(y-1),0);			// Replace empty block
				}

				// Top
				if (canAbove && (rand()%2) == 0)
				{
					if (player->board[x][y-1] == 0)
						replaceBlockOnBoard(player,(x),(y-1),(rand()%8));	// Replace random Block
					else
						replaceBlockOnBoard(player,(x),(y-1),0);				// Replace empty block
				}

				// Top Right
				if (canAbove && canRight && (rand()%2) == 0)
				{
					if (player->board[x+1][y-1] == 0)
						replaceBlockOnBoard(player,(x+1),(y-1),(rand()%8));	// Replace random Block
					else
						replaceBlockOnBoard(player,(x+1),(y-1),0);			// Replace empty block
				}

				// Mid left
				if (canLeft && (rand()%2) == 0)
				{
					if (player->board[x-1][y] == 0)
						replaceBlockOnBoard(player,(x-1),(y),(rand()%8));	// Replace random Block
					else
						replaceBlockOnBoard(player,(x-1),(y),0);				// Replace empty block
				}

				// Mid (Bomb Block)
				replaceBlockOnBoard(player,(x),(y),0);						// Replace empty block


				// Mid Right
				if (canRight && (rand()%2) == 0)
				{
					if (player->board[x+1][y] == 0)
						replaceBlockOnBoard(player,(x+1),(y),(rand()%8));	// Replace random Block
					else
						replaceBlockOnBoard(player,(x+1),(y),0);				// Replace empty block
				}

				// Bottom left
				if (canBelow && canLeft && (rand()%2) == 0)
				{
					if (player->board[x-1][y+1] == 0)
						replaceBlockOnBoard(player,(x-1),(y+1),(rand()%8));	// Replace random Block
					else
						replaceBlockOnBoard(player,(x-1),(y+1),0);			// Replace empty block
				}

				// Bottom
				if (canBelow && (rand()%2) == 0)
				{
					if (player->board[x][y+1] == 0)
						replaceBlockOnBoard(player,(x),(y+1),(rand()%8));	// Replace random Block
					else
						replaceBlockOnBoard(player,(x),(y+1),0);				// Replace empty block
				}

				// Bottom Right
				if (canBelow && canRight && (rand()%2) == 0)
				{
					if (player->board[x+1][y+1] == 0)
						replaceBlockOnBoard(player,(x+1),(y+1),(rand()%8));	// Replace random Block
					else
						replaceBlockOnBoard(player,(x+1),(y+1),0);			// Replace empty block
				}
			}
		}
	}
}

// Replaces the block at the specified position with the specified replacement block
void replaceBlockOnBoard(PlayerData *player, int pos_x, int pos_y, BlockColour replacement)
{
	// Replace on board
	player->board[pos_x][pos_y] = replacement;

	// Replace Tilemap
	if (replacement > 0)
	{
		player->boardTileMap[ pos_x ].tiles[ pos_y ].block_number = ((PTILE)(&blocks16[ (int)player->board[pos_x][pos_y] ]))->block_number;
		player->boardTileMap[ pos_x ].tiles[ pos_y ].attributes = ((PTILE)(&blocks16[ (int)player->board[pos_x][pos_y] ]))->attributes;
	}
	else
	{
		player->boardTileMap[ pos_x ].tiles[ pos_y ].block_number = 0;
		player->boardTileMap[ pos_x ].tiles[ pos_y ].attributes = 0;
	}
}

// drop the next special block in the players inventory
void dropCurrentSpecialBlock(PlayerData *player)
{
	// verify we even have any blocks
	if (player->specialBlocks[0] == 0)
		return;

	// remove block
	removeFirstBlockFromInventory(player);

	// Now Draw Inventory Blocks
	drawSpecialBlockInventories();
}

// Removes first block from Special Block Inventory, pushes each block up one
void removeFirstBlockFromInventory(PlayerData *player)
{
	int x;

	for (x=1; x<6; x++)
	{
		player->specialBlocks[x-1] = player->specialBlocks[x];
	}
	// Last block must be 0
	player->specialBlocks[5] = 0;
}

// Draw the players special block inventory
void drawSpecialBlockInventories()
{
	int x;

	// Set Sprite
	set_current_sprite(SPRITE_INVENTORIES);

	// Draw Player 1 Inventory
	for (x=0; x<6; x++)
	{
		if (gPlayer1Data.specialBlocks[x] > 0)
		{
			write_sprite_data( (x*BLOCK_WIDTH)+PLAYER_1_INVENTORY_LEFT, PLAYER_1_INVENTORY_TOP,15,255,1,1,(const PTILEMAP)&blocks12[(int)gPlayer1Data.specialBlocks[x]]);
		}
		else
		{
			// Done w/ this Player
			write_sprite_data( (x*BLOCK_WIDTH)+PLAYER_1_INVENTORY_LEFT, PLAYER_1_INVENTORY_TOP,15,255,1,1,(const PTILEMAP)&blocks12[0]);
			x = 6;
		}
	}

	// Draw Player 2 Inventory
	for (x=0; x<6; x++)
	{
		if (gPlayer2Data.specialBlocks[x] > 0)
		{
			//write_sprite_data( (x*BLOCK_WIDTH)+PLAYER_2_INVENTORY_LEFT, PLAYER_2_INVENTORY_TOP, 11, 191, 1, 1, (PTILEMAP)(blocks16[ (int)gPlayer2Data.specialBlocks[x] ]) );
			write_sprite_data( (x*BLOCK_WIDTH)+PLAYER_2_INVENTORY_LEFT, PLAYER_2_INVENTORY_TOP,15,255,1,1,(const PTILEMAP)&blocks12[(int)gPlayer2Data.specialBlocks[x]]);
		}
		else
		{
			// Done w/ this Player
			x = 6;
		}
	}
}

void setGameEvent(WORD event)
{
	gGameData.event |= event;
}

void clearGameEvents()
{
	gGameData.event = EVENT_NONE;
}

void displayMenu()
{
	// User Input
	DWORD input = 0x0;			// Control Input
	int loopCount = 0;			// For seeding randomizer
	int player_select = 1;		// 1 = 1 player, 2 = 2 player
	int difficulty_select = 1;	// 1 = easy, 2 = med, 3 = hard
	int menu_select = 1;		// 1 = player select, 2 = difficulty select
	int pointerFlashTimer = 60;	// Flash it between 0 and 10 ticks.
	int pointerX, pointerY;		// The pointer arrow.

	// Clear all sprites
	clear_spr();

	// And all text.
	clear_fix();

	// Set Current Sprite to 1
	set_current_sprite( SPRITE_BACKGROUND );

	// Display BG Tilemap
	write_sprite_data(0, 0, 15, 240, 32, 19, (const PTILEMAP)&menuscreen);

	// Write Player Select Arrow Sprite
	set_current_sprite( SPRITE_BLOCK );
	write_sprite_data(43,72,15,255,1,1, (const PTILEMAP)&arrow);

	// Write difficulty Select Arrow Sprite
	set_current_sprite( SPRITE_BLOCK+1 );
	write_sprite_data(179,72,15,255,1,1, (const PTILEMAP)&arrow);

	pointerX = 43; pointerY = 72;

	// Loop until user makes selection
	while (!(input & JOY_A))
	{
		input = poll_joystick(PORT1,READ_BIOS_CHANGE);
		if (menu_select == 1)
		{
			// Player Select Menu

			if (input & JOY_DOWN)
			{
				// Select 2 player
				player_select = 2;

				// Move Sprite to 2 player
				pointerX = 43;
				pointerY = 91;

				pointerFlashTimer = 60;
			}
			else if (input & JOY_UP)
			{
				// Select 1 player
				player_select = 1;

				// Move Sprite to 1 player
				pointerX = 43;
				pointerY = 72;

				pointerFlashTimer = 60;
			}
			if (input & JOY_RIGHT)
			{
				// Move to Difficulty Select Menu
				menu_select = 2;
				difficulty_select = 1;
				pointerFlashTimer = 0;
			}

			pointerFlashTimer++;
			pointerFlashTimer = (pointerFlashTimer % 60);
			if ( pointerFlashTimer > 35 )
			{
				change_sprite_pos( 20, -20, -20, 1 );
			}
			else
			{
				change_sprite_pos( 20, pointerX, pointerY, 1 );
			}

			if (input & JOY_RIGHT)
			{
				// Move to Difficulty Select Menu
				pointerX = 179; pointerY = 72;
			}
		}
		else
		{
			// Difficulty Select Menu
			if (input & JOY_DOWN)
			{
				switch (difficulty_select)
				{
					case 1:
						// Move to 2
						difficulty_select = 2;
						pointerX = 179; pointerY = 87;
						break;
					case 2:
						// Move to 3
						difficulty_select = 3;
						pointerX = 179; pointerY = 101;
						break;
					default:
						break;
				}
				pointerFlashTimer = 60;
			}
			else if (input & JOY_UP)
			{
				switch (difficulty_select)
				{
					case 2:
						// Move to 1
						difficulty_select = 1;
						pointerX = 179; pointerY = 72;
						break;
					case 3:
						// Move to 2
						difficulty_select = 2;
						pointerX = 179; pointerY = 87;
						break;
					default:
						break;
				}
				pointerFlashTimer = 60;
			}
			else if (input & JOY_LEFT)
			{
				// move to player select menu
				menu_select = 1;
				player_select = 1;
			}

			pointerFlashTimer++;
			pointerFlashTimer = (pointerFlashTimer % 60);
			if ( pointerFlashTimer > 35 )
			{
				change_sprite_pos( 21, -20, -20, 0 );
			}
			else
			{
				change_sprite_pos( 21, pointerX, pointerY, 1 );
			}

			if (input & JOY_LEFT)
			{
				// move to player select menu
				pointerFlashTimer = 0;
				pointerX = 43; pointerY = 72;
			}
		}

		// Increment Loop Counter
		loopCount++;

		wait_vbl();
	}

	// Seed Randomizer with loop count
	srand(loopCount);

	// Set Game Difficulty
	gGameData.difficulty = (difficulty_select - 1);

	// Return Number of Players
	gGameData.numPlayers = player_select;
}

PlayerData *getOppositePlayer(PlayerID id)
{
	if (id == PLAYER_ONE)
		return &gPlayer2Data;
	else
		return &gPlayer1Data;
}

