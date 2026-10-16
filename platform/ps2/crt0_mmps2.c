/*
 * Minimal EE crt0 for the MM port (replaces the 8-instruction _start that
 * lived in the generated build/ee/ps2_stubs.c).
 *
 * Deliberately has NO #includes: it is compiled in the decomp include world
 * (compile_new.sh) where newlib headers are shadowed.  Platform objects are
 * linked before ps2_stubs.o and the link uses --allow-multiple-definition,
 * so this _start wins.
 *
 * Does, in order:
 *   1. $gp = _gp (ps2sdk libraries are built -mabicalls/-G0 against it)
 *   2. $sp = _mmps2_stack_top (stack carved out in linkfile, not the
 *      loader's, so Main_InitMemory's RAM wipe cannot hit it)
 *   3. zero [_fbss, _end_bss)  -- ELF loaders do not reliably clear .bss
 *   4. jal mmps2_crt0_main -> bootproc()
 */
__asm__(
    ".section .text\n"
    ".set push\n"
    ".set noreorder\n"
    ".globl _start\n"
    ".type _start, @function\n"
    "_start:\n"
    "    la   $gp, _gp\n"
    "    la   $sp, _mmps2_stack_top\n"
    "    addiu $sp, $sp, -64\n"
    "    la   $t0, _fbss\n"
    "    la   $t1, _end_bss\n"
    "1:  sltu $t2, $t0, $t1\n"
    "    beqz $t2, 2f\n"
    "    nop\n"
    "    sw   $zero, 0($t0)\n"
    "    b    1b\n"
    "    addiu $t0, $t0, 4\n"
    "2:  jal  mmps2_crt0_main\n"
    "    nop\n"
    "3:  b    3b\n"
    "    nop\n"
    ".size _start, .-_start\n"
    ".set pop\n"
);

extern void bootproc(void);
extern char _end[];
extern void mmps2_scr(const char *fmt, ...);

void mmps2_crt0_main(void)
{
    mmps2_scr("crt0: gp/sp/bss ok -> bootproc");
    bootproc();
    for (;;) {
    }
}

/*
 * Heap for newlib malloc (gsKit allocates through it).  Window is
 * [_end, _end + MMPS2_HEAP_RESERVE); the linkfile excludes the same window
 * from Main_InitMemory()'s wipe.  Keep the size in sync with
 * MMPS2_HEAP_RESERVE in platform/ps2/linkfile.
 */
#define MMPS2_HEAP_BYTES 0x200000

void *_sbrk(int incr)
{
    static char *cur;
    char *old;

    if (cur == 0)
        cur = _end;
    old = cur;
    if (cur + incr > _end + MMPS2_HEAP_BYTES || cur + incr < _end)
        return (void *)-1;
    cur += incr;
    return old;
}
