#include <stdint.h>
extern int usb_console_init(void);
extern int button_pressed(void);
extern void show_status(const char *text);
int main(void) {
 usb_console_init();
 for (;;) if (button_pressed()) show_status("Volunteer confirmed check; enter inventory in site system");
}
