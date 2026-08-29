EESchema Schematic File Version 7
        EELAYER 0 0
        EELAYER END
        $Descr A4 11700 8268
        encoding utf-8
        Sheet 1 1
        Title "OutageSync Resilience Hub"
        Date "2026-08-29"
        Rev "1.0"
        Comp "Devices"
        Comment1 "CM4 + RP2040 + SX1262 + EG25-G"
        Comment2 "Rails: 12V, 5V, 3V3"
        Comment3 "Buses: SPI, I2C, UART, DSI, USB"
        Comment4 "Local dashboard and LTE fallback"
        $EndDescr
        $Comp
        L Device:U U1
        U 1 1 66000001
        P 3800 2400
        F 0 "U1" H 3800 3200 50 0000 C CNN
        F 1 "RP2040" H 3800 3100 50 0000 C CNN
        F 2 "Module:RP2040" H 3800 2400 50 0001 C CNN
        F 3 "" H 3800 2400 50 0001 C CNN
        1 3800 2400
        1 0 0 -1
        $EndComp
        $Comp
        L Device:U U2
        U 1 1 66000002
        P 7300 2400
        F 0 "U2" H 7300 3200 50 0000 C CNN
        F 1 "Raspberry_Pi_CM4" H 7300 3100 50 0000 C CNN
        F 2 "Module:CM4" H 7300 2400 50 0001 C CNN
        F 3 "" H 7300 2400 50 0001 C CNN
        1 7300 2400
        1 0 0 -1
        $EndComp
        Text Notes 1200 6900 0 50 ~ 0
        SPI0 -> SX1262
UART0 -> CM4 heartbeat
USB2 -> EG25-G LTE modem
DSI -> 7in touch panel
I2C -> RTC + INA219
        $EndSCHEMATC
