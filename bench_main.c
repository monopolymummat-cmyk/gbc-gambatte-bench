/* bench_main.c — headless benchmark ROM for independent frame-count
 * reproduction of the published GBC figures (bounty #16517).
 *
 * Links ONLY the Q8 runtime + model chunks (no UI, no tokenizer, no vocab),
 * per the published benchmark description: "links neither the UI nor the
 * tokenizer nor the vocabulary".
 *
 * Protocol (work RAM, CGB single-CPU WRAM bank at 0xC000):
 *   0xC000  magic    0xB1  (bench alive / started)
 *   0xC001  done     0x00 while running, 0x2A on completion
 *   0xC002  count    number of tokens produced so far (0..16)
 *   0xC010  tokens   16 x uint16 little-endian greedy argmax outputs
 *   0xC030  seed     0x0001 (start token id 1, published protocol)
 */
#include <gb/gb.h>
#include <stdint.h>
#include <stddef.h>

#include "gbc_q8_chunks.h"
#include "gbc_q8_runtime.h"

#define BENCH_TOKENS 16

#define WRAM_MAGIC  0xB1u
#define WRAM_DONE   0x2Au

volatile uint8_t *bench_magic  = (volatile uint8_t *)0xC000;
volatile uint8_t *bench_done   = (volatile uint8_t *)0xC001;
volatile uint8_t *bench_count  = (volatile uint8_t *)0xC002;
volatile uint16_t *bench_tokens = (volatile uint16_t *)0xC010;
volatile uint16_t *bench_seed  = (volatile uint16_t *)0xC030;

void gbc_q8_progress(uint8_t layer, uint8_t stage) {
    /* headless: no-op. Kept so the runtime links unchanged. */
    (void)layer; (void)stage;
}

void main(void) {
    gbc_q8_model_t model;
    uint16_t token;
    uint16_t next;
    uint8_t pos;

    *bench_magic = WRAM_MAGIC;
    *bench_done  = 0x00;
    *bench_count = 0;
    *bench_seed  = 1;

    /* The runtime switches the CGB core to double speed during init
     * (cpu_fast() on CGB_TYPE) — identical to the demo ROM path. */
    (void)gbc_q8_init(&model);
    gbc_q8_reset();

    token = 1; /* published protocol: "All 16 greedy tokens from token id 1" */
    for (pos = 0; pos < BENCH_TOKENS; pos++) {
        next = gbc_q8_forward_argmax(&model, token, pos);
        bench_tokens[pos] = next;
        *bench_count = (uint8_t)(pos + 1);
        token = next;
    }

    *bench_done = WRAM_DONE;
    for (;;) {
        wait_vbl_done();
    }
}
