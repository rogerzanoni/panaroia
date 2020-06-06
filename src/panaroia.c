#include "panaroia/panaroia.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#include "log.h"

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define pnria_trace(...) log_log(LOG_TRACE, __FILENAME__, __LINE__, __VA_ARGS__)
#define pnria_debug(...) log_log(LOG_DEBUG, __FILENAME__, __LINE__, __VA_ARGS__)
#define pnria_info(...)  log_log(LOG_INFO,  __FILENAME__, __LINE__, __VA_ARGS__)
#define pnria_warn(...)  log_log(LOG_WARN,  __FILENAME__, __LINE__, __VA_ARGS__)
#define pnria_error(...) log_log(LOG_ERROR, __FILENAME__, __LINE__, __VA_ARGS__)
#define pnria_fatal(...) log_log(LOG_FATAL, __FILENAME__, __LINE__, __VA_ARGS__)

static pnria_state_t chip8;

pnria_state_t pnria_get_state()
{
    return chip8;
}

void pnria_set_input(const char *key)
{
#if !defined(NDEBUG)
    for (int i = 0; i < 16; ++i) {
        if (chip8.key[i]) {
            pnria_debug("Key[%d]: pressed", i);
        }
    }
#endif
    memcpy(chip8.key, key, 16);
}

unsigned char *pnria_get_screen()
{
    return chip8.screen;
}

// handlers: take a pointer to an instruction and pass the correct arguments

typedef void (*pnria_nnn_function_t)(unsigned short value);
void pnria_nnn_handler(unsigned short opcode, pnria_nnn_function_t instruction)
{
    unsigned short nnn = chip8.opcode & 0x0FFF;
    pnria_debug("Argument(nnn): 0x%X", nnn);
    instruction(nnn);
}

typedef void (*pnria_xkk_function_t)(unsigned short x, unsigned char k);
void pnria_xkk_handler(unsigned short opcode, pnria_xkk_function_t instruction)
{
    unsigned short x = (chip8.opcode & 0x0F00) >> 8;
    char k = chip8.opcode & 0x00FF;
    pnria_debug("Arguments(x, kk): 0x%X, 0x%X", x, k);
    instruction(x, k);
}

typedef void (*pnria_xyn_function_t)(unsigned short x, unsigned short y, unsigned short n);
void pnria_xyn_handler(unsigned short opcode, pnria_xyn_function_t instruction)
{
    unsigned short x = (chip8.opcode & 0x0F00) >> 8;
    unsigned short y = (chip8.opcode & 0x00F0) >> 4;
    unsigned short n = chip8.opcode & 0x000F;
    pnria_debug("Arguments(x, y, n): 0x%X, 0x%X, 0x%X", x, y, n);
    instruction(x, y, n);
}

typedef void (*pnria_xy_function_t)(unsigned short x, unsigned short y);
void pnria_xy_handler(unsigned short opcode, pnria_xy_function_t instruction)
{
    unsigned short x = (chip8.opcode & 0x0F00) >> 8;
    unsigned short y = (chip8.opcode & 0x00F0) >> 4;
    pnria_debug("Arguments(x, y): 0x%X, 0x%X", x, y);
    instruction(x, y);
}

typedef void (*pnria_x_function_t)(unsigned short x);
void pnria_x_handler(unsigned short opcode, pnria_x_function_t instruction)
{
    unsigned short x = (chip8.opcode & 0x0F00) >> 8;
    pnria_debug("Argument(x): 0x%X", x);
    instruction(x);
}

// clear screen
static void pnria_00e0()
{
    pnria_debug("00E0");
    memset(chip8.screen, 0, PNRIA_SCREEN_SIZE);
}

// return from subroutine
static void pnria_00ee()
{
    pnria_debug("00EE");
    --chip8.SP;
    chip8.PC = chip8.stack[chip8.SP];
    chip8.PC += PNRIA_OPCODE_SIZE;
}

// jump to NNN
static void pnria_1nnn(unsigned short nnn)
{
    pnria_debug("1NNN, nnn: %X", nnn);
    chip8.PC = nnn;
}

// call NNN
static void pnria_2nnn(unsigned short nnn)
{
    pnria_debug("2NNN, nnn: %X", nnn);
    // store PC - opcode size because PC incremented in pnria_execute
    chip8.stack[chip8.SP] = chip8.PC - PNRIA_OPCODE_SIZE;
    ++chip8.SP;
    chip8.PC = nnn;
}

#define SKIPIF(condition) {                       \
    if (condition) chip8.PC += PNRIA_OPCODE_SIZE; \
}

// skip next instruction if Vx == kk
static void pnria_3xkk(unsigned short x, unsigned char kk)
{
    pnria_debug("3XKK, x: %X, kk: %X", x, kk);
    SKIPIF(chip8.V[x] == kk);
}

// skip next instruction if Vx != kk
static void pnria_4xkk(unsigned short x, unsigned char kk)
{
    pnria_debug("4XKK, x: %X, kk: %X", x, kk);
    SKIPIF(chip8.V[x] != kk);
}

// skip next instruction if Vx == Vy
static void pnria_5xy0(unsigned short x, unsigned short y)
{
    pnria_debug("5XY0, x: %X, y: %X", x, y);
    SKIPIF(chip8.V[x] == chip8.V[y]);
}

// load kk into Vx
static void pnria_6xkk(unsigned short x, unsigned char kk)
{
    pnria_debug("6XKK, x: %X, kk: %X", x, kk);
    chip8.V[x] = kk;
}

// add kk to Vx
static void pnria_7xkk(unsigned short x, unsigned char kk)
{
    pnria_debug("7XKK, x: %X, kk: %X", x, kk);
    chip8.V[x] += kk;
}

// set Vx = Vy
static void pnria_8xy0(unsigned short x, unsigned short y)
{
    pnria_debug("8XY0, x: %X, y: %X", x, y);
    chip8.V[x] = chip8.V[y];
}

// set Vx = Vx OR Vy
static void pnria_8xy1(unsigned short x, unsigned short y)
{
    pnria_debug("8XY1, x: %X, y: %X", x, y);
    chip8.V[x] |= chip8.V[y];
}

// set Vx = Vx AND Vy
static void pnria_8xy2(unsigned short x, unsigned short y)
{
    pnria_debug("8XY2, x: %X, y: %X", x, y);
    chip8.V[x] &= chip8.V[y];
}

// set Vx = Vx XOR Vy
static void pnria_8xy3(unsigned short x, unsigned short y)
{
    pnria_debug("8XY3, x: %X, y: %X", x, y);
    chip8.V[x] ^= chip8.V[y];
}

// set Vx = Vx + Vy, if sum is greater than the capacity, set V[F] to carry
static void pnria_8xy4(unsigned short x, unsigned short y)
{
    pnria_debug("8XY4, x: %X, y: %X", x, y);
    chip8.V[x] += chip8.V[y];
    chip8.V[0xF] = chip8.V[y] > (0xFF - chip8.V[x]) ? 1 : 0;
}

// set Vx = Vx - Vy, if Vx > Vy, set V[F] to NOT borrow
static void pnria_8xy5(unsigned short x, unsigned short y)
{
    pnria_debug("8XY5, x: %X, y: %X", x, y);
    chip8.V[0xF] = chip8.V[y] > chip8.V[x] ? 0 : 1;
    chip8.V[x] -= chip8.V[y];
}

// if LSB of Vx is 1, V[F] is set to 1, them Vx is divided by 2 (SHR)
static void pnria_8xy6(unsigned short x, unsigned short y)
{
    pnria_debug("8XY6, x: %X, y: %X", x, y);
    chip8.V[0xF] = chip8.V[x] & 0x0001;
    chip8.V[x] >>= 1;
}

// set Vx = Vy - Vx, set V[F] to NOT borrow
static void pnria_8xy7(unsigned short x, unsigned short y)
{
    pnria_debug("8XY7, x: %X, y: %X", x, y);
    chip8.V[0xF] = chip8.V[x] > chip8.V[y] ? 0 : 1;
    chip8.V[x] = chip8.V[y] - chip8.V[x];
}

// if LSB of Vx is 1, V[F] is set to 1, them Vx is multiplied by 2 (SHL)
static void pnria_8xye(unsigned short x, unsigned short y)
{
    pnria_debug("8XYE, x: %X, y: %X", x, y);
    chip8.V[0xF] = chip8.V[x] >> 7;
    chip8.V[x] <<= 1;
}

// skip next instruction if Vx != Vy
static void pnria_9xy0(unsigned short x, unsigned short y)
{
    pnria_debug("9XY0, x: %X, y: %X", x, y);
    SKIPIF(chip8.V[x] != chip8.V[y]);
}

// set index register to NNN
static void pnria_annn(unsigned short nnn)
{
    pnria_debug("ANNN, nnn: %X", nnn);
    chip8.I = nnn;
}

// jump to NNN + V0
static void pnria_bnnn(unsigned short nnn)
{
    pnria_debug("BNNN, nnn: %X, V0:", nnn, chip8.V[0]);
    chip8.PC = nnn + chip8.V[0];
}

// set Vx to a random byte AND kk
static void pnria_cxkk(unsigned short x, unsigned char kk)
{
    pnria_debug("CXKK, x: %X, kk: %X, x, kk");
    chip8.V[x] = (rand() % 0X100) & kk;
}

// draw a sprite of n bytes at xy position in the screen
static void pnria_dxyn(unsigned short x, unsigned short y, unsigned short n)
{
    pnria_debug("DXYN, x: %X, y: %X, n: %X", x, y, n);
    unsigned short screenX = chip8.V[x];
    unsigned short screenY = chip8.V[y];

    chip8.V[0xF] = 0;

    // n is the sprite height
    for (int spriteY = 0; spriteY < n; ++spriteY) {
        unsigned short spriteLine = chip8.memory[chip8.I + spriteY];
        // every sprite is 8 bits wide
        for (int spriteX = 0; spriteX < 8; ++spriteX) {
            // from MSB to LSB, for each sprite line we check if it's already set and update Vx
            if ((spriteLine & (0x80 >> spriteX)) != 0) { // if the sprite pixel is set
                // get the screen pixel that can be changed
                unsigned short screenPixel = (screenX + spriteX + ((screenY + spriteY) * 64));

                if (screenPixel >= PNRIA_SCREEN_SIZE) {
                    continue;
                }

                if (chip8.screen[screenPixel] == 1) { // and screen pixel is set
                    chip8.V[0xF] = 1; // collision
                }

                chip8.screen[screenPixel] ^= 1;
            }
        }
    }
}

// skip next instruction if Vx is pressed
static void pnria_ex9e(unsigned short x)
{
    pnria_debug("EX9E, x: %X", x);
    SKIPIF(chip8.key[chip8.V[x]] != 0);
}

// skip next instruction if Vx is not pressed
static void pnria_exa1(unsigned short x)
{
    pnria_debug("EXA1, x: %X", x);
    SKIPIF(chip8.key[chip8.V[x]] == 0);
}

// set Vx to the delay value
static void pnria_fx07(unsigned short x)
{
    pnria_debug("FX07, x: %X", x);
    chip8.V[x] = chip8.delay;
}

// wait for a keypress, store result in x
static void pnria_fx0a(unsigned short x)
{
    pnria_debug("FX0A, x: %X", x);
    bool keyPressed = false;
    for (int i = 0; i < PNRIA_INPUT_SIZE; ++i) {
        if (chip8.key[i] != 0) {
            chip8.V[x] = i;
            keyPressed = true;
            break;
        }
    }

    if (!keyPressed) {
        chip8.PC -= PNRIA_OPCODE_SIZE;
    }
}

// set delay to Vx
static void pnria_fx15(unsigned short x)
{
    pnria_debug("FX15, x: %X", x);
    chip8.delay = chip8.V[x];
}

// set sound to Vx
static void pnria_fx18(unsigned short x)
{
    pnria_debug("FX18, x: %X", x);
    chip8.sound = chip8.V[x];
}

// set I to I + Vx
static void pnria_fx1e(unsigned short x)
{
    pnria_debug("FX1E, x: %X", x);
    chip8.V[0xF] = (chip8.I + chip8.V[x] > 0xFFF) ? 1 : 0;
    chip8.I += chip8.V[x];
}

// set I to the address of the starting pos of font digit in Vx
static void pnria_fx29(unsigned short x)
{
    pnria_debug("FX29, x: %X", x);
    chip8.I = chip8.V[x] * 5;
}

// store the BCD represantation of Vx into I, I+1, I+2
static void pnria_fx33(unsigned short x)
{
    pnria_debug("FX33, x: %X", x);
    chip8.memory[chip8.I]     = chip8.V[x] / 100;
    chip8.memory[chip8.I + 1] = (chip8.V[x] / 10) % 10;
    chip8.memory[chip8.I + 2] = chip8.V[x] % 10;
}

// stores registers V0 through Vx into memory, starting at I
static void pnria_fx55(unsigned short x)
{
    pnria_debug("FX55, x: %X", x);
    for (int i = 0; i <= x; ++i) {
        chip8.memory[chip8.I + i] = chip8.V[i];
    }
}

// read registers V0 through Vx, storing at memory starting at I
static void pnria_fx65(unsigned short x)
{
    pnria_debug("FX65, x: %X", x);
    for (int i = 0; i <= x; ++i) {
        chip8.V[i] = chip8.memory[chip8.I + i];
    }
}

// instruction table

static void pnria_unknown()
{
    pnria_warn("Unknown instruction, opcode: %X", chip8.opcode);
}

typedef void (*pnria_func_t)(void);
typedef enum { X, XY, XYN, XKK, NNN, NOARGS } pnria_argument_t;

typedef struct {
    pnria_argument_t type;
    pnria_func_t instruction;
} pnria_handler_t;

static void pnria_dispatch(pnria_argument_t type, pnria_func_t instruction)
{
    pnria_debug("Dispatching... instruction: %p", instruction);

    if (instruction == NULL) {
        type = NOARGS;
        instruction = pnria_unknown;
    }

    if (type == NOARGS) {
        pnria_debug("No arguments, calling handler...");
        instruction();
    } else if (type == X) {
        pnria_x_handler(chip8.opcode, (pnria_x_function_t)instruction);
    } else if (type == XY) {
        pnria_xy_handler(chip8.opcode, (pnria_xy_function_t)instruction);
    } else if (type == XYN) {
        pnria_xyn_handler(chip8.opcode, (pnria_xyn_function_t)instruction);
    } else if (type == XKK) {
        pnria_xkk_handler(chip8.opcode, (pnria_xkk_function_t)instruction);
    } else if (type == NNN) {
        pnria_nnn_handler(chip8.opcode, (pnria_nnn_function_t)instruction);
    }
}

#define PNRIA_HANDLER(type, instruction) (pnria_handler_t){ type, (pnria_func_t)instruction }

static pnria_handler_t pnria_0table[0x10];
static void pnria_0handler()
{
    pnria_debug("getting function at %X", chip8.opcode & 0x000F);
    pnria_dispatch(NOARGS, pnria_0table[chip8.opcode & 0x000F].instruction);
}

static pnria_handler_t pnria_8table[0x10];
static void pnria_8handler()
{
    pnria_dispatch(XY, pnria_8table[chip8.opcode & 0x000F].instruction);
}

static pnria_handler_t pnria_etable[0x10];
static void pnria_ehandler()
{
    pnria_dispatch(X, pnria_etable[chip8.opcode & 0x000F].instruction);
}

static pnria_handler_t pnria_ftable[0X100];
static void pnria_fhandler()
{
    pnria_dispatch(X, pnria_ftable[chip8.opcode & 0x00FF].instruction);
}

static pnria_handler_t pnria_table[0X10] = {
    PNRIA_HANDLER(NOARGS, pnria_0handler),
    PNRIA_HANDLER(NNN,    pnria_1nnn),
    PNRIA_HANDLER(NNN,    pnria_2nnn),
    PNRIA_HANDLER(XKK,    pnria_3xkk),
    PNRIA_HANDLER(XKK,    pnria_4xkk),
    PNRIA_HANDLER(XY,     pnria_5xy0),
    PNRIA_HANDLER(XKK,    pnria_6xkk),
    PNRIA_HANDLER(XKK,    pnria_7xkk),
    PNRIA_HANDLER(NOARGS, pnria_8handler),
    PNRIA_HANDLER(XY,     pnria_9xy0),
    PNRIA_HANDLER(NNN,    pnria_annn),
    PNRIA_HANDLER(NNN,    pnria_bnnn),
    PNRIA_HANDLER(XKK,    pnria_cxkk),
    PNRIA_HANDLER(XYN,    pnria_dxyn),
    PNRIA_HANDLER(NOARGS, pnria_ehandler),
    PNRIA_HANDLER(NOARGS, pnria_fhandler)
};

static void pnria_execute()
{
    unsigned short index = (chip8.opcode & 0xF000) >> 12;

    pnria_debug("Executing opcode 0x%X, function index 0x%X", chip8.opcode, index);

    pnria_handler_t handler = pnria_table[index];
    chip8.PC += PNRIA_OPCODE_SIZE;
    pnria_dispatch(handler.type, handler.instruction);
}

void pnria_init()
{
#if defined(NDEBUG)
    log_set_level(LOG_INFO);
#else
    // Warning: debug builds will generate huge logs
    log_set_level(LOG_DEBUG);
#endif

    log_info("Initializing...");

    chip8.PC     = PNRIA_START_OFFSET;
    chip8.opcode = 0;
    chip8.I      = 0;
    chip8.SP     = 0;
    chip8.delay  = 0;
    chip8.sound  = 0;

    // load fontset
    unsigned char fontset[] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
        0x20, 0x60, 0x20, 0x20, 0x70, // 1
        0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
        0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
        0x90, 0x90, 0xF0, 0x10, 0x10, // 4
        0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
        0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
        0xF0, 0x10, 0x20, 0x40, 0x40, // 7
        0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
        0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
        0xF0, 0x90, 0xF0, 0x90, 0x90, // A
        0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
        0xF0, 0x80, 0x80, 0x80, 0xF0, // C
        0xE0, 0x90, 0x90, 0x90, 0xE0, // D
        0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
        0xF0, 0x80, 0xF0, 0x80, 0x80  // F
    };

    memset(chip8.memory, 0,       PNRIA_MEMORY_SIZE);
    memcpy(chip8.memory, fontset, 80);
    memset(chip8.stack,  0,       PNRIA_STACK_SIZE);
    memset(chip8.V,      0,       PNRIA_REGISTER_SIZE);
    memset(chip8.key,    0,       PNRIA_INPUT_SIZE);
    memset(chip8.screen, 0,       PNRIA_SCREEN_SIZE);

    srand (time(NULL));

    pnria_0table[0x0] = PNRIA_HANDLER(NOARGS, pnria_00e0);
    pnria_0table[0xe] = PNRIA_HANDLER(NOARGS, pnria_00ee);

    pnria_8table[0x0] = PNRIA_HANDLER(XY, pnria_8xy0);
    pnria_8table[0x1] = PNRIA_HANDLER(XY, pnria_8xy1);
    pnria_8table[0x2] = PNRIA_HANDLER(XY, pnria_8xy2);
    pnria_8table[0x3] = PNRIA_HANDLER(XY, pnria_8xy3);
    pnria_8table[0x4] = PNRIA_HANDLER(XY, pnria_8xy4);
    pnria_8table[0x5] = PNRIA_HANDLER(XY, pnria_8xy5);
    pnria_8table[0x6] = PNRIA_HANDLER(XY, pnria_8xy6);
    pnria_8table[0x7] = PNRIA_HANDLER(XY, pnria_8xy7);
    pnria_8table[0xe] = PNRIA_HANDLER(XY, pnria_8xye);

    pnria_etable[0x1] = PNRIA_HANDLER(X, pnria_exa1);
    pnria_etable[0xe] = PNRIA_HANDLER(X, pnria_ex9e);

    pnria_ftable[0x07] = PNRIA_HANDLER(X, pnria_fx07);
    pnria_ftable[0x0A] = PNRIA_HANDLER(X, pnria_fx0a);
    pnria_ftable[0x15] = PNRIA_HANDLER(X, pnria_fx15);
    pnria_ftable[0x18] = PNRIA_HANDLER(X, pnria_fx18);
    pnria_ftable[0x1E] = PNRIA_HANDLER(X, pnria_fx1e);
    pnria_ftable[0x29] = PNRIA_HANDLER(X, pnria_fx29);
    pnria_ftable[0x33] = PNRIA_HANDLER(X, pnria_fx33);
    pnria_ftable[0x55] = PNRIA_HANDLER(X, pnria_fx55);
    pnria_ftable[0x65] = PNRIA_HANDLER(X, pnria_fx65);
}

void pnria_reset()
{
    log_info("Resetting...");
    chip8 = (pnria_state_t) {};
    pnria_init();
}

void pnria_cycle()
{
    if (chip8.PC >= PNRIA_MEMORY_SIZE) {
        return;
    }

    chip8.opcode = chip8.memory[chip8.PC] << 8 | chip8.memory[chip8.PC + 1];

    pnria_execute();

    if (chip8.delay > 0) {
        pnria_debug("Decrementing delay timer, value: ", chip8.delay);
        --chip8.delay;
    }

    if (chip8.sound > 0) {
        pnria_debug("Decrementing sound timer, value: ", chip8.sound);
        --chip8.sound;
    }
}

bool pnria_load(const char *romFile)
{
    if (!romFile) {
        pnria_warn("No rom file name. Provide the path to the rom file.");
        return false;
    }

    pnria_info("Loading %s...", romFile);

    FILE *file = fopen(romFile, "r");
    if (!file) {
        pnria_error("Error reading file: %s", strerror(errno));
        return false;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);

    char buffer[size];
    long read_size = fread(buffer, sizeof(char), size, file);

    if (size != read_size) {
        pnria_error("Read size != file size.");
        fclose(file);
        return false;
    }

    if ((PNRIA_START_OFFSET + size) > PNRIA_MEMORY_SIZE) {
        pnria_error("ROM bigger than the available memory, %d bytes. Aborting.", size);
        fclose(file);
        return false;
    }

    memcpy(chip8.memory + PNRIA_START_OFFSET, buffer, size);

    pnria_info("Rom loaded, %d bytes read", size);

    fclose(file);

    return true;
}
