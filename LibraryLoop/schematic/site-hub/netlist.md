# Hub pin-level netlist (conceptual, not KiCad)

Pico W GP0 UART TX → MAX3485 DI; GP1 ← RO; GP2 → DE and /RE tied; A/B to protected twisted pair; optional 120 Ω across A/B at bus end. Common signal reference and shield termination require site electrical review. 5 V USB supplies Pico board; 3.3 V logic. Display SDA/SCL GP4/GP5. Add TVS at connector, local decoupling, fuse and labeled test points. Board-level schematic/layout not supplied.
