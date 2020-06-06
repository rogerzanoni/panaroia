#ifndef PANAROIA_CONTROLLER_H
#define PANAROIA_CONTROLLER_H

#include <string>
#include <array>

#include <SDL_keycode.h>

class PanaroiaController {
public:
    PanaroiaController();

    void step();
    void reset();

    void keyUp(SDL_Keycode keycode);
    void keyDown(SDL_Keycode keycode);

    void setCurrentRom(const std::string &rom);
    const std::string &currentRom() const;

    bool running() const;

    unsigned char *screen() const;

    char inputState(int index) const;
    SDL_Keycode keyMapping(int index) const;

private:
    void init();
    void updateInputState(SDL_Keycode keycode, bool pressed);

private:
    std::string m_currentRom;
    std::array<SDL_Keycode, 16> m_keymap;
    bool m_running;
    char m_chip8Keys[16] = { 0 };
};

#endif // PANAROIA_CONTROLLER_H
