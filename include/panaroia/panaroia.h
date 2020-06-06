#ifndef PANAROIA_H
#define PANAROIA_H

#include <stdbool.h>

#define PNRIA_START_OFFSET  0x200
#define PNRIA_OPCODE_SIZE   2
#define PNRIA_SCREEN_SIZE 2048 // display size 64 * 32
#define PNRIA_MEMORY_SIZE 4096
#define PNRIA_STACK_SIZE 16
#define PNRIA_INPUT_SIZE 16
#define PNRIA_REGISTER_SIZE 16

#ifdef __cplusplus
extern "C" {
#endif

// chip8 state
typedef struct {
    // current opcode
    unsigned short opcode;

    // chip's memory
    unsigned char memory[PNRIA_MEMORY_SIZE];

    // general purpose registers
    unsigned char V[PNRIA_REGISTER_SIZE];

    // index register
    unsigned short I;

    // program counter
    unsigned short PC;

    // graphics output
    unsigned char screen[PNRIA_SCREEN_SIZE];

    // timers
    unsigned char delay;
    unsigned char sound;

    // stack and stack pointer
    unsigned short stack[PNRIA_STACK_SIZE];
    unsigned short SP;

    // keypad input state
    unsigned char key[PNRIA_INPUT_SIZE];

    // indicates if the system is waiting for a keypress
    // if it's greater than 0, waits for the keypress and use the
    // value as register index for storing the key value
    int waiting_for_key;
} pnria_state_t;


void pnria_init();
void pnria_reset();
void pnria_cycle();
bool pnria_load(const char *romFile);
void pnria_set_input(const char *key);
unsigned char *pnria_get_screen();
pnria_state_t pnria_get_state();

#ifdef __cplusplus
}
#endif

#endif // PANAROIA_H
