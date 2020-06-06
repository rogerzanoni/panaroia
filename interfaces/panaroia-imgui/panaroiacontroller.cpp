#include "panaroiacontroller.h"

#include "panaroia/panaroia.h"

PanaroiaController::PanaroiaController()
    : m_running{false}
{
    init();

    m_keymap = {
        SDLK_u, SDLK_q, SDLK_w, SDLK_e,
        SDLK_a, SDLK_s, SDLK_d, SDLK_z,
        SDLK_x, SDLK_c, SDLK_y, SDLK_i,
        SDLK_r, SDLK_f, SDLK_v, SDLK_o
    };
}

void PanaroiaController::init()
{
    pnria_init();
}

void PanaroiaController::step()
{
    pnria_cycle();
    pnria_set_input(m_chip8Keys);
}

void PanaroiaController::reset()
{
    pnria_reset();
    if (!m_currentRom.empty()) {
        pnria_load(m_currentRom.c_str());
    }
}

void PanaroiaController::setCurrentRom(const std::string &rom)
{
    if (rom.empty()) {
        return;
    }
    m_currentRom = rom;
    reset();
    pnria_load(m_currentRom.c_str());
    m_running = true;
}

const std::string &PanaroiaController::currentRom() const
{
    return m_currentRom;
}

bool PanaroiaController::running() const
{
    return m_running;
}

unsigned char *PanaroiaController::screen() const
{
    return pnria_get_screen();
}

char PanaroiaController::inputState(int index) const
{
    if (index < 0 || index >= 16) {
        return 0;
    }
    return m_chip8Keys[index];
}

SDL_Keycode PanaroiaController::keyMapping(int index) const
{
    return m_keymap[index];
}

void PanaroiaController::updateInputState(SDL_Keycode keycode, bool pressed)
{
    for (int i = 0; i < 16; ++i) {
        if (m_keymap[i] == keycode) {
            m_chip8Keys[i] = pressed ? 1 : 0;
            break;
        }
    }
}

void PanaroiaController::keyUp(SDL_Keycode keycode)
{
    updateInputState(keycode, false);
}

void PanaroiaController::keyDown(SDL_Keycode keycode)
{
    updateInputState(keycode, true);
}
