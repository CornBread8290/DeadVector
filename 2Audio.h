#ifndef AUDIO_H
#define AUDIO_H

enum SoundType {
    SND_BEEP = 0,
    SND_ENGINE,
    SND_GUN,
    SND_HARSH
};

void playSoundEffect(int type, float pitch, float volume, float pan, int channel);
void stopSound(int channel);

#endif // AUDIO_H
