#include <stdio.h>

#include "panaroia/panaroia.h"

#include <check.h>

#define TEST_ROM_NAME "test-rom.chip8"

void write_test_rom(char* rom, unsigned short size)
{
    FILE *f = fopen(TEST_ROM_NAME, "w");
    ck_assert_ptr_ne(f, NULL);

    size_t written = fwrite(rom, sizeof(char), size, f);
    if (written != size) {
        perror("Error writing test rom file.");
    }
    fclose(f);
}

#define LOAD_ROM(...) {                                        \
    short *list = (short[]) { __VA_ARGS__, -1 };               \
    int len = 0;                                               \
    while (*(list + len) != -1) ++len;                         \
    unsigned short data[len];                                  \
    for (int i = 0; i < len; ++i) {                            \
        unsigned short val = list[i];                          \
        data[i] = (val & 0x00FF) << 8 | (val & 0xFF00) >> 8;   \
    }                                                          \
    write_test_rom((char*)data, len * 2);                      \
    pnria_reset();                                             \
    ck_assert(pnria_load(TEST_ROM_NAME));                      \
}

#define EXECUTE_INSTRUCTION(instruction) ({ \
        LOAD_ROM(instruction);              \
        pnria_cycle();                      \
        pnria_get_state();                  \
})

#define EXECUTE_INSTRUCTIONS(cycles, ...) ({ \
        LOAD_ROM(__VA_ARGS__);               \
        for (int i = 0; i < cycles; ++i) {   \
            pnria_cycle();                   \
        }                                    \
        pnria_get_state();                   \
})

static void check_init()
{
    pnria_state_t state = pnria_get_state();

    ck_assert_uint_eq(state.PC,     PNRIA_START_OFFSET);
    ck_assert_uint_eq(state.opcode, 0);
    ck_assert_uint_eq(state.I,      0);
    ck_assert_uint_eq(state.SP,     0);
    ck_assert_uint_eq(state.delay,  0);
    ck_assert_uint_eq(state.sound,  0);

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

    for (int i = 0; i < 80; ++i) {
        ck_assert_uint_eq(state.memory[i], fontset[i]);
    }
}

START_TEST (init_state_test)
{
    pnria_init();

    check_init();
}
END_TEST

START_TEST (reset_state_test)
void reset_state_test()
{
    pnria_reset();

    check_init();
}
END_TEST

START_TEST (load_test)
{
    LOAD_ROM(0x0123, 0x4567);

    pnria_state_t state = pnria_get_state();

    ck_assert_uint_eq(state.memory[PNRIA_START_OFFSET],     0x01);
    ck_assert_uint_eq(state.memory[PNRIA_START_OFFSET + 1], 0x23);
    ck_assert_uint_eq(state.memory[PNRIA_START_OFFSET + 2], 0x45);
    ck_assert_uint_eq(state.memory[PNRIA_START_OFFSET + 3], 0x67);

    size_t size = PNRIA_MEMORY_SIZE * 10;
    char hugeRom[size];
    memset(hugeRom, 1, size);
    write_test_rom(hugeRom, size);
    ck_assert(!pnria_load(TEST_ROM_NAME));

    size_t availableSize = PNRIA_MEMORY_SIZE - PNRIA_START_OFFSET;
    char bigRom[availableSize];
    memset(bigRom, 1, availableSize);
    write_test_rom(bigRom, availableSize);
    ck_assert(pnria_load(TEST_ROM_NAME));

    size_t exceedingSize = PNRIA_MEMORY_SIZE - PNRIA_START_OFFSET + 1;
    char exceedingRom[exceedingSize];
    memset(exceedingRom, 1, exceedingSize);
    write_test_rom(exceedingRom, exceedingSize);
    ck_assert(!pnria_load(TEST_ROM_NAME));
}
END_TEST

START_TEST (test_1nnn)
{
    pnria_state_t state = EXECUTE_INSTRUCTION(0x1123);
    ck_assert_uint_eq(state.PC, 0x123);
}
END_TEST

START_TEST (test_2nnn)
{
    pnria_state_t state = EXECUTE_INSTRUCTION(0x2321);
    ck_assert_uint_eq(state.PC, 0x321);
    ck_assert_uint_eq(state.SP, 1);
    ck_assert_uint_eq(state.stack[state.SP - 1], PNRIA_START_OFFSET);
}
END_TEST

START_TEST (test_00ee)
{
    pnria_state_t state = EXECUTE_INSTRUCTIONS(
        2,
        0x2202,
        0x00EE
    );
    ck_assert_uint_eq(state.PC, PNRIA_START_OFFSET + PNRIA_OPCODE_SIZE);
    ck_assert_uint_eq(state.SP, 0);
}
END_TEST

START_TEST (test_annn)
{
    pnria_state_t state = EXECUTE_INSTRUCTION(0xA123);
    ck_assert_uint_eq(state.I, 0x123);
    ck_assert_uint_eq(state.PC, PNRIA_START_OFFSET + PNRIA_OPCODE_SIZE);
}
END_TEST

START_TEST (test_bnnn)
{
    pnria_state_t state = EXECUTE_INSTRUCTION(0xB321);
    ck_assert_uint_eq(state.PC, 0x321);
}
END_TEST

START_TEST (test_3xkk)
{
    // don't skip
    pnria_state_t state = EXECUTE_INSTRUCTION(0x3001);
    ck_assert_uint_eq(state.PC, PNRIA_START_OFFSET + PNRIA_OPCODE_SIZE);

    // skip
    state = EXECUTE_INSTRUCTION(0x3000);
    ck_assert_uint_eq(state.PC, PNRIA_START_OFFSET + PNRIA_OPCODE_SIZE * 2);
}
END_TEST

START_TEST (test_4xkk)
{
    // skip
    pnria_state_t state = EXECUTE_INSTRUCTION(0x4001);
    ck_assert_uint_eq(state.PC, PNRIA_START_OFFSET + PNRIA_OPCODE_SIZE * 2);

    // don't skip
    state = EXECUTE_INSTRUCTION(0x4000);
    ck_assert_uint_eq(state.PC, PNRIA_START_OFFSET + PNRIA_OPCODE_SIZE);
}
END_TEST

START_TEST (test_6xkk)
{
    pnria_state_t state = EXECUTE_INSTRUCTIONS(
        16,
        0x6000, 0x6101, 0x6202, 0x6303,
        0x6404, 0x6505, 0x6606, 0x6707,
        0x6808, 0x6909, 0x6A0A, 0x6B0B,
        0x6C0C, 0x6D0D, 0x6E0E, 0x6F0F
    );

    for (int i = 0; i < 16; ++i) {
        ck_assert_uint_eq(state.V[i], i);
    }
}
END_TEST

START_TEST (test_7xkk)
{
    pnria_state_t state = EXECUTE_INSTRUCTIONS(
        32,
        0x7001, 0x7102, 0x7203, 0x7304,
        0x7405, 0x7506, 0x7607, 0x7708,
        0x7809, 0x790A, 0x7A0B, 0x7B0C,
        0x7C0D, 0x7D0E, 0x7E0F, 0x7F10,
        0x7001, 0x7102, 0x7203, 0x7304,
        0x7405, 0x7506, 0x7607, 0x7708,
        0x7809, 0x790A, 0x7A0B, 0x7B0C,
        0x7C0D, 0x7D0E, 0x7E0F, 0x7F10
    );

    for (int i = 0; i < 16; ++i) {
        ck_assert_uint_eq(state.V[i], (i+1)*2);
    }
}
END_TEST

START_TEST (test_5xy0)
{
    pnria_state_t state = EXECUTE_INSTRUCTIONS(
        5,
        0x6101, 0x6301, 0x6402,
        0x5140, // don't skip
        0x5130  // skip
    );

    ck_assert_uint_eq(state.PC, PNRIA_START_OFFSET + PNRIA_OPCODE_SIZE * 6);
}
END_TEST

START_TEST (test_8xy0)
{
    pnria_state_t state = EXECUTE_INSTRUCTIONS(
        16,
        0x600F, // load F into V[0]
        0x8100, 0x8210, 0x8320, 0x8430,
        0x8540, 0x8650, 0x8760, 0x8870,
        0x8980, 0x8A90, 0x8BA0, 0x8CB0,
        0x8DC0, 0x8ED0, 0x8FE0
    );

    for (int i = 0; i < 16; ++i) {
        ck_assert_uint_eq(state.V[i], 0xF);
    }
}
END_TEST

START_TEST (test_8xy1)
{
    pnria_state_t state = EXECUTE_INSTRUCTIONS(
        16,
        0x60FF, // load F into V[0]
        0x8101, 0x8211, 0x8321, 0x8431,
        0x8541, 0x8651, 0x8761, 0x8871,
        0x8981, 0x8A91, 0x8BA1, 0x8CB1,
        0x8DC1, 0x8ED1, 0x8FE1
    );

    for (int i = 0; i < 16; ++i) {
        ck_assert_uint_eq(state.V[i], 0xFF);
    }
}
END_TEST

START_TEST (test_8xy2)
{
    pnria_state_t state = EXECUTE_INSTRUCTIONS(
        32,
        0x60FF, 0x61FF, 0x62FF, 0x63FF,
        0x64FF, 0x65FF, 0x66FF, 0x67FF,
        0x68FF, 0x69FF, 0x6AFF, 0x6BFF,
        0x6CFF, 0x6DFF, 0x6EFF, 0x6F00,

        0x80F2, 0x81F2, 0x82F2, 0x83F2,
        0x84F2, 0x85F2, 0x86F2, 0x87F2,
        0x88F2, 0x89F2, 0x8AF2, 0x8BF2,
        0x8CF2, 0x8DF2, 0x8EF2, 0x8FF2
    );

    for (int i = 0; i < 16; ++i) {
        ck_assert_uint_eq(state.V[i], 0x00);
    }
}
END_TEST

START_TEST (test_8xy3)
{
    pnria_state_t state = EXECUTE_INSTRUCTIONS(
        32,
        0x60AA, 0x61AA, 0x62AA, 0x63AA,
        0x64AA, 0x65AA, 0x66AA, 0x67AA,
        0x68AA, 0x69AA, 0x6AAA, 0x6BAA,
        0x6CAA, 0x6DAA, 0x6EAA, 0x6F5A,

        0x80F3, 0x81F3, 0x82F3, 0x83F3,
        0x84F3, 0x85F3, 0x86F3, 0x87F3,
        0x88F3, 0x89F3, 0x8AF3, 0x8BF3,
        0x8CF3, 0x8DF3, 0x8EF3, 0x8FF3
    );

    for (int i = 0; i < 15; ++i) {
        ck_assert_uint_eq(state.V[i], 0xF0);
    }
    ck_assert_uint_eq(state.V[0xF], 0);
}
END_TEST

START_TEST (test_8xy4)
    pnria_state_t state = EXECUTE_INSTRUCTIONS(
        3,
        0x600A, 0x610A,
        0x8014
    );

    ck_assert_uint_eq(state.V[0], 20);
    ck_assert_uint_eq(state.V[0xF], 0);

    state = EXECUTE_INSTRUCTIONS(
        3,
        0x6AFF, 0x6BFF,
        0x8AB4
    );

    ck_assert_uint_eq(state.V[0xA], 0xFE);
    ck_assert_uint_eq(state.V[0xF], 1);
END_TEST

START_TEST (test_8xy5)
    pnria_state_t state = EXECUTE_INSTRUCTIONS(
        3,
        0x600A, 0x610A,
        0x8015
    );

    ck_assert_uint_eq(state.V[0], 0);
    ck_assert_uint_eq(state.V[0xF], 1);

    state = EXECUTE_INSTRUCTIONS(
        3,
        0x6A00, 0x6BFF,
        0x8AB5
    );

    ck_assert_uint_eq(state.V[0xA], 1);
    ck_assert_uint_eq(state.V[0xF], 0);
END_TEST

START_TEST (test_8xy6)
    pnria_state_t state = EXECUTE_INSTRUCTIONS(
        2,
        0x600A,
        0x8006
    );

    ck_assert_uint_eq(state.V[0], 5);
    ck_assert_uint_eq(state.V[0xF], 0);

    state = EXECUTE_INSTRUCTIONS(
        2,
        0x6011,
        0x8006
    );

    ck_assert_uint_eq(state.V[0x0], 8);
    ck_assert_uint_eq(state.V[0xF], 1);
END_TEST

START_TEST (test_8xy7)
    pnria_state_t state = EXECUTE_INSTRUCTIONS(
        3,
        0x600A, 0x610A,
        0x8017
    );

    ck_assert_uint_eq(state.V[0], 0);
    ck_assert_uint_eq(state.V[0xF], 1);

    return;

    state = EXECUTE_INSTRUCTIONS(
        3,
        0x6A00, 0x6101,
        0x8017
    );

    ck_assert_uint_eq(state.V[0xA], 255);
    ck_assert_uint_eq(state.V[0xF], 0);
END_TEST

START_TEST (test_8xye)
    pnria_state_t state = EXECUTE_INSTRUCTIONS(
        2,
        0x600A,
        0x800e
    );

    ck_assert_uint_eq(state.V[0], 20);
    ck_assert_uint_eq(state.V[0xF], 0);

    state = EXECUTE_INSTRUCTIONS(
        2,
        0x60F0,
        0x800e
    );

    ck_assert_uint_eq(state.V[0], 224);
    ck_assert_uint_eq(state.V[0xF], 1);
END_TEST

START_TEST (test_9xy0)
{
    pnria_state_t state = EXECUTE_INSTRUCTIONS(
        5,
        0x600A, 0x610A, 0x620F,
        0x9010, 0x9020
    );

    ck_assert_uint_eq(state.PC, PNRIA_START_OFFSET + PNRIA_OPCODE_SIZE * 6);
}
END_TEST

START_TEST (test_ex9e)
{
    // don't skip
    pnria_state_t state = EXECUTE_INSTRUCTIONS(
        2,
        0x600A,
        0xE09E
    );

    ck_assert_uint_eq(state.PC, PNRIA_START_OFFSET + PNRIA_OPCODE_SIZE * 2);

    // skip
    char keys[16] = { 0 };
    keys[0xA] = 1;

    // don't skip
    LOAD_ROM(0x600A, 0xE09E)
    pnria_set_input(keys);
    pnria_cycle(); pnria_cycle();
    state = pnria_get_state();

    ck_assert_uint_eq(state.PC, PNRIA_START_OFFSET + PNRIA_OPCODE_SIZE * 3);
}
END_TEST

START_TEST (test_exa1)
{
    // don't skip
    pnria_state_t state = EXECUTE_INSTRUCTIONS(
        2,
        0x600A,
        0xE0A1
    );

    ck_assert_uint_eq(state.PC, PNRIA_START_OFFSET + PNRIA_OPCODE_SIZE * 3);

    // don't skip
    char keys[16] = { 0 };
    keys[0xA] = 1;

    // skip
    LOAD_ROM(0x600A, 0xE0A1)
    pnria_set_input(keys);
    pnria_cycle(); pnria_cycle();
    state = pnria_get_state();

    ck_assert_uint_eq(state.PC, PNRIA_START_OFFSET + PNRIA_OPCODE_SIZE * 2);
}
END_TEST

START_TEST (test_fx15)
{
    pnria_state_t state = EXECUTE_INSTRUCTIONS(
        2,
        0x6051, 0xF015
    );

    // delay decreases in the cycle
    ck_assert_uint_eq(state.delay, 0x50);
}
END_TEST

START_TEST (test_fx07)
{
    pnria_state_t state = EXECUTE_INSTRUCTIONS(
        3,
        0x6051, 0xF015, 0xF007
    );

    ck_assert_uint_eq(state.V[0], 0x50);
    ck_assert_uint_eq(state.delay, 0x4F);
}
END_TEST

START_TEST (test_fx18)
{
    pnria_state_t state = EXECUTE_INSTRUCTIONS(
        2,
        0x6051, 0xF018
    );

    ck_assert_uint_eq(state.sound, 0x50);
}
END_TEST

START_TEST (test_fx1e)
{
    pnria_state_t state = EXECUTE_INSTRUCTIONS(
        2,
        0x60AA, 0xF01E
    );

    ck_assert_uint_eq(state.I, 0xAA);
}
END_TEST

START_TEST (test_fx55)
{
    pnria_state_t state = EXECUTE_INSTRUCTIONS(
        18,
        // load all registers
        0x6000, 0x6101, 0x6202, 0x6303,
        0x6404, 0x6505, 0x6606, 0x6707,
        0x6808, 0x6909, 0x6A0A, 0x6B0B,
        0x6C0C, 0x6D0D, 0x6E0E, 0x6F0F,

        0xAAAA, // set I to AAA

        0xFF55 // load all registers into memory
    );

    for (int i = 0; i < 16; ++i) {
        ck_assert_uint_eq(state.memory[0xAAA + i], i);
    }
}
END_TEST

START_TEST (test_fx65)
{
    pnria_state_t state = EXECUTE_INSTRUCTIONS(
        3,
        0x1212, // jump to the instruction after test data

        // test data
        0x0102, 0x0304, 0x0506, 0x0708,
        0x090A, 0x0B0C, 0x0D0E, 0x0F10,

        0xA202, // point I to start of test data

        0xFF65 // load test data to registers
    );

    for (int i = 0; i < 16; ++i) {
        ck_assert_uint_eq(state.V[i], i + 1);
    }
}
END_TEST

START_TEST (test_fx33)
{
    pnria_state_t state = EXECUTE_INSTRUCTIONS(
        3,
        0x60AC, 0xABBB, 0xF033
    );

    ck_assert_uint_eq(state.I, 0xBBB);
    ck_assert_uint_eq(state.memory[state.I],     1); // 0001
    ck_assert_uint_eq(state.memory[state.I + 1], 7); // 0111
    ck_assert_uint_eq(state.memory[state.I + 2], 2); // 0010
}
END_TEST

Suite *panaroia_suite()
{
    Suite *suite = suite_create("panaroia");
    TCase *core = tcase_create("Core");

    // lib interface
    tcase_add_test(core, init_state_test);
    tcase_add_test(core, reset_state_test);
    tcase_add_test(core, load_test);

    // TODO 0xe0
    // TODO 0xee

    // instructions
    tcase_add_test(core, test_1nnn);
    tcase_add_test(core, test_2nnn);
    tcase_add_test(core, test_00ee);
    tcase_add_test(core, test_annn);
    tcase_add_test(core, test_bnnn);

    tcase_add_test(core, test_3xkk);
    tcase_add_test(core, test_6xkk);
    tcase_add_test(core, test_7xkk);

    // TODO cxkk

    tcase_add_test(core, test_5xy0);
    tcase_add_test(core, test_8xy0);
    tcase_add_test(core, test_8xy1);
    tcase_add_test(core, test_8xy2);
    tcase_add_test(core, test_8xy3);
    tcase_add_test(core, test_8xy4);
    tcase_add_test(core, test_8xy5);
    tcase_add_test(core, test_8xy6);
    tcase_add_test(core, test_8xy7);
    tcase_add_test(core, test_8xye);
    tcase_add_test(core, test_9xy0);

    tcase_add_test(core, test_ex9e);
    tcase_add_test(core, test_exa1);
    tcase_add_test(core, test_fx15);
    tcase_add_test(core, test_fx07);
    tcase_add_test(core, test_fx18);
    tcase_add_test(core, test_fx1e);

    // TODO fx29

    // TODO dxy0

    tcase_add_test(core, test_fx55);
    tcase_add_test(core, test_fx65);
    tcase_add_test(core, test_fx33);

    suite_add_tcase(suite, core);

    return suite;
}

int main()
{
    int failed;
    Suite *s = panaroia_suite();
    SRunner *runner = srunner_create(s);

    srunner_run_all(runner, CK_VERBOSE);
    failed = srunner_ntests_failed(runner);
    srunner_free(runner);

    return failed == 0 ? 0 : 1;
}
