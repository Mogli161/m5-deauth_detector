#ifndef GIF_PLAYER_H
#define GIF_PLAYER_H

#include <Arduino.h>
#include <AnimatedGIF.h>

// Plays the firmware-embedded menu GIF on the Cardputer screen in a loop
// until the user presses Esc (backtick key). Self-contained blocking call,
// mirrors the style of Display::showAnimatedIntro().
class GifPlayer {
public:
    GifPlayer();
    void playUntilEsc();

private:
    AnimatedGIF gif;
    static void GIFDraw(GIFDRAW *pDraw);
};

#endif
