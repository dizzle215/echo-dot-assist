/* buttons.c against a FIFO standing in for the keypad: a short press of the action button is a press; a key held for a
 * second is a hold, reported once and before the key is let go, and letting go after a hold is not a press; the volume
 * keys step on the press and hold too, and the kernel's key repeat changes nothing.  Beside them the older gestures
 * keep working: the action button's reset stages (5 s warn, let go = no reset) and both volume keys held 2 s (pair),
 * which is not a hold of either key. */
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "board.h"
#include "buttons.h"

const struct board board = { .keypad = NULL };     /* one input device, no second keypad, no latch */

static int bad;
static void check(int ok, const char *what) { printf("%s %s\n", ok ? "ok  " : "FAIL", what); if (!ok) bad = 1; }

static int n_action, n_action_hold, n_up, n_down, n_up_hold, n_down_hold, n_mute, n_pair, n_stage1, n_stage0, n_stage2;
static void action(void) { n_action++; }
static void action_hold(void) { n_action_hold++; }
static void mute_changed(int m) { (void)m; n_mute++; }
static void volume(int d) { if (d > 0) n_up++; else n_down++; }
static void volume_hold(int d) { if (d > 0) n_up_hold++; else n_down_hold++; }
static void pair(void) { n_pair++; }
static void hold(int stage) { if (stage == 1) n_stage1++; else if (stage == 0) n_stage0++; else n_stage2++; }

static int wfd;
static void key(int code, int value)
{
    struct input_event ev; memset(&ev, 0, sizeof ev);
    ev.type = EV_KEY; ev.code = code; ev.value = value;
    if (write(wfd, &ev, sizeof ev) != sizeof ev) { perror("write"); exit(1); }
}
static void ms(int n) { usleep(n * 1000); }

int main(void)
{
    char dir[] = "/tmp/buttons_test.XXXXXX", fifo[64];
    if (!mkdtemp(dir)) { perror("mkdtemp"); return 1; }
    snprintf(fifo, sizeof fifo, "%s/event1", dir);
    if (mkfifo(fifo, 0600)) { perror("mkfifo"); return 1; }
    wfd = open(fifo, O_RDWR);                       /* a writer first: the reader's open would wait for one */
    if (wfd < 0) { perror("open"); return 1; }

    static const struct button_handler h = { .action = action, .mute_changed = mute_changed, .volume = volume,
                                             .pair = pair, .hold = hold, .action_hold = action_hold, .volume_hold = volume_hold };
    check(buttons_start(fifo, &h) == 0, "reader started on the FIFO");

    key(KEY_HELP, 1); ms(100); key(KEY_HELP, 0); ms(200);
    check(n_action == 1 && n_action_hold == 0, "a short press of the action button is one press");

    key(KEY_HELP, 1); ms(600);
    check(n_action_hold == 0, "600 ms down is not a hold yet");
    ms(700);
    check(n_action_hold == 1, "a second down: the hold is reported before the key is let go");
    key(KEY_HELP, 0); ms(200);
    check(n_action == 1 && n_action_hold == 1 && n_stage1 == 0, "letting go after a hold is neither a press nor a second hold");

    key(KEY_VOLUMEUP, 1); ms(100); key(KEY_VOLUMEUP, 0); ms(200);
    check(n_up == 1 && n_up_hold == 0, "volume up: one step on the press");

    key(KEY_VOLUMEDOWN, 1); ms(300); key(KEY_VOLUMEDOWN, 2); ms(1000);
    check(n_down == 1 && n_down_hold == 1, "volume down held: the step on the press, one hold, key repeat ignored");
    key(KEY_VOLUMEDOWN, 0); ms(200);
    check(n_down == 1 && n_down_hold == 1, "letting the volume key go adds nothing");

    key(KEY_HELP, 1); ms(100); key(KEY_HELP, 0); ms(200);
    check(n_action == 2 && n_action_hold == 1, "a press after a hold is a press again");
    check(n_mute == 0, "nothing reported for mute: no latch and no mute key here");

    /* the reset stages still come: the 1 s hold first, the ring's warning at 5 s, let go = no reset */
    key(KEY_HELP, 1); ms(1300);
    check(n_action_hold == 2 && n_stage1 == 0, "held again: the 1 s hold, no reset warning yet");
    ms(4000);
    check(n_stage1 == 1 && n_stage2 == 0, "5 s down: the reset warning stage, once");
    key(KEY_HELP, 0); ms(200);
    check(n_stage0 == 1 && n_action == 2 && n_action_hold == 2, "let go before 10 s: 'no reset' told, no press, no hold");

    /* both volume keys for 2 s is the pairing gesture, not two holds; their presses still stepped once each way */
    key(KEY_VOLUMEUP, 1); ms(50); key(KEY_VOLUMEDOWN, 1); ms(150); key(KEY_VOLUMEDOWN, 2); ms(1300);
    check(n_up == 2 && n_down == 2 && n_up_hold == 0 && n_down_hold == 1 && n_pair == 0, "both down 1.5 s: no volume hold, no pair yet");
    key(KEY_VOLUMEDOWN, 2); ms(800);
    check(n_pair == 1, "both down 2 s: pair, once");
    key(KEY_VOLUMEUP, 0); key(KEY_VOLUMEDOWN, 0); ms(200);
    check(n_up_hold == 0 && n_down_hold == 1 && n_pair == 1, "let go: still no volume hold from the pairing gesture");

    close(wfd); unlink(fifo); rmdir(dir);
    return bad;
}
