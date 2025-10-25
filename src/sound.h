#ifndef XASTIR_SOUND_H
#define XASTIR_SOUND_H

#include <sys/types.h>

#define SPEECH_TEST_STRING "Greeteengz frum eggzaster"

extern pid_t play_sound(char *sound_cmd, char *soundfile);
extern int sound_done(void);

#endif // XASTIR_SOUND_H