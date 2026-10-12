/* Top buttons: GPIO keypad (board.keypad) and the hardware mute latch (board.privacy_*); donut: docs/re-platform.md. */
#ifndef BUTTONS_H
#define BUTTONS_H

struct button_handler {
    void (*action)(void);               /* short press of the action button */
    void (*mute_changed)(int muted);    /* hardware privacy latch changed */
    void (*volume)(int direction);      /* +1 / -1, also on key repeat */
    void (*pair)(void);                 /* may be NULL: Volume up and Volume down held together for 2 s */
    void (*hold)(int stage);            /* may be NULL: action button held: 1 at 5 s, 2 at 10 s; 0 let go in between */
    void (*action_hold)(void);          /* may be NULL: action button down for HOLD_MS, once, while it is still down;
                                           letting go afterwards is not a press (the reset stages still follow) */
    void (*volume_hold)(int direction); /* may be NULL: the same for a volume key, after volume() reported its press;
                                           not while the other volume key is down too (that is the pairing gesture) */
};

#define HOLD_MS 1000                    /* a key down this long is a hold; the action button's longer holds are the reset
                                           stages (HOLD_WARN_MS, HOLD_RESET_MS in buttons.c) */

/* Starts a reader thread.  Returns -1 if the input device cannot be opened (PC build: always). */
int  buttons_start(const char *device, const struct button_handler *h);
int  buttons_muted(void);               /* current privacy state, 0 if unknown */
#endif
