# Hub wiring and assembly

The gateway is an off-the-shelf Raspberry Pi Zero 2 W board powered through its USB input from a certified regulated 5 V supply. No bare mains wiring. Radio antenna is integrated; install in a non-metallic enclosure with clearance. MicroSD contains local API/database. Network interfaces should bind to localhost by default; remote dashboard exposure requires authenticated TLS and firewalling.

This uses a module-level assembly description, not a custom KiCad schematic. No ERC, thermal, or RF validation has been performed.
