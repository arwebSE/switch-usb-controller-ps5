#include "usb_controllers.h"
#include <assert.h>
#include <stdio.h>

static pad_state_t parse(const uint8_t *report, size_t len)
{
    pad_state_t st;
    assert(usb_parse_input_report(CTRL_PDP_FACEOFF, report, len, &st));
    return st;
}

int main(void)
{
    const char *name = NULL;
    assert(usb_identify_controller(0x0e6f, 0x0184, &name) == CTRL_PDP_FACEOFF);
    assert(usb_identify_controller(0x0e6f, 0x0185, &name) == CTRL_UNKNOWN);

    uint8_t report[8] = {0, 0, 15, 128, 128, 128, 128, 0};
    pad_state_t st = parse(report, sizeof(report));
    assert(st.buttons == 0 && st.lx == 128 && st.ry == 128);
    assert(!usb_parse_input_report(CTRL_PDP_FACEOFF, report, 6, &st));

    report[0] = 0x04; /* Switch B -> Cross */
    report[1] = 0x10; /* Home -> PS */
    report[2] = 0x02; /* Right */
    report[3] = 0;
    report[4] = 255;
    st = parse(report, sizeof(report));
    assert((st.buttons & (PAD_BTN_CROSS | PAD_BTN_PS | PAD_DPAD_RIGHT)) ==
           (PAD_BTN_CROSS | PAD_BTN_PS | PAD_DPAD_RIGHT));
    assert(st.lx == 0 && st.ly == 255);

    report[0] = 0xc0;
    report[1] = 0;
    report[2] = 15;
    st = parse(report, sizeof(report));
    assert((st.buttons & (PAD_BTN_L2 | PAD_BTN_R2)) == (PAD_BTN_L2 | PAD_BTN_R2));
    assert(st.l2 == 255 && st.r2 == 255);
    puts("PDP parser tests passed");
    return 0;
}
