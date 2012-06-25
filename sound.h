// FM/ADPCM HEADER FILE created by MVSTracker

inline void send_sound_command(BYTE CommandNo)
{
	(*((PBYTE)0x320000)) = CommandNo;
}

// FM Music commands
#define SOUND_FM_OFF	(0xff)

// ADPCM Sample commands
#define SOUND_ADPCM_GAME_START	(0x04)
#define SOUND_ADPCM_ROTATE_BLOCK	(0x05)
#define SOUND_ADPCM_CLEAR_ONE_LINE	(0x06)
#define SOUND_ADPCM_CLEAR_FOUR_LINES	(0x07)
#define SOUND_ADPCM_CLEAR_LINE_WITH_SPECIAL_BLOCK	(0x08)
#define SOUND_ADPCM_BLOCK_HITS_BOTTOM	(0x09)
#define SOUND_ADPCM_BLOCK_DROP	(0x0a)
#define SOUND_ADPCM_OFF	(0x7f)

