/* Optional context-node integration skeleton; report equipment context only. */
#define CONTEXT_I2C_SDA_GPIO 4
#define CONTEXT_I2C_SCL_GPIO 5
#define SHT40_I2C_ADDRESS 0x44
int main(void) { return CONTEXT_I2C_SDA_GPIO == CONTEXT_I2C_SCL_GPIO; }
