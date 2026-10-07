#include "GifPlayer.h"
#include "assets/MenuGif.h"
#include <M5Cardputer.h>

#define GIF_X_OFFSET 60
#define GIF_Y_OFFSET 7

static uint16_t lineBuf[480];

GifPlayer::GifPlayer() {}

void GifPlayer::GIFDraw(GIFDRAW *pD)
{
    uint8_t *s = pD->pPixels;
    uint16_t *palette = pD->pPalette;
    int y = pD->iY + pD->y + GIF_Y_OFFSET;

    if (pD->y == 0) {
        // nothing special needed per-frame start
    }

    int width = pD->iWidth;
    if (width > 480) width = 480;

    if (pD->ucHasTransparency) {
        uint8_t transparentIdx = pD->ucTransparent;
        int x = 0;
        while (x < width) {
            int xStart = x;
            while (x < width && s[x] == transparentIdx) x++;
            if (x > xStart) {
                // skip transparent run (leave background as-is)
            }
            xStart = x;
            while (x < width && s[x] != transparentIdx) {
                lineBuf[x] = palette[s[x]];
                x++;
            }
            if (x > xStart) {
                M5Cardputer.Display.pushImage(pD->iX + xStart + GIF_X_OFFSET, y,
                                               x - xStart, 1, &lineBuf[xStart]);
            }
        }
    } else {
        for (int x = 0; x < width; x++) {
            lineBuf[x] = palette[s[x]];
        }
        M5Cardputer.Display.pushImage(pD->iX + GIF_X_OFFSET, y, width, 1, lineBuf);
    }
}

void GifPlayer::playUntilEsc()
{
    M5Cardputer.Display.fillScreen(BLACK);
    M5Cardputer.Display.setTextColor(WHITE, BLACK);
    M5Cardputer.Display.setCursor(5, 125);
    M5Cardputer.Display.print("Esc to go back");

    gif.begin(GIF_PALETTE_RGB565_LE);

    if (!gif.open((uint8_t *)MENU_GIF_DATA, MENU_GIF_SIZE, GIFDraw)) {
        M5Cardputer.Display.setCursor(10, 60);
        M5Cardputer.Display.print("GIF load failed!");
        delay(1500);
        return;
    }

    bool escPressed = false;
    while (!escPressed) {
        int result = gif.playFrame(true, NULL);
        if (result == 0) {
            // loop finished, restart playback
            gif.reset();
        }

        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
            Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
            for (char c : status.word) {
                if (c == '`') {
                    escPressed = true;
                }
            }
        }
    }

    gif.close();
}
