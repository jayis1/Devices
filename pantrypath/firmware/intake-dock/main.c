#define UART_TX 0
#define UART_RX 1
#define ENCODER_A 14
#define ENCODER_B 15
#define NODE_ID 3
/* RP2040 volunteer-operated dock; explicit confirmation required. */
int valid_category(unsigned char category){return category<16;}
