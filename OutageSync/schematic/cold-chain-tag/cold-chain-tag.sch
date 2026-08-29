EESchema Schematic File Version 7
        EELAYER 0 0
        EELAYER END
        $Descr A4 11700 8268
        encoding utf-8
        Sheet 1 1
        Title "OutageSync Cold Chain Tag"
        Date "2026-08-29"
        Rev "1.0"
        Comp "Devices"
        Comment1 "nRF52840 + SX1262 + TMP117 + SHT45"
        Comment2 "CR2477 / AAA freezer-safe sensing"
        Comment3 "Door reed + BLE commissioning"
        Comment4 "Fridge / freezer / medicine cooler telemetry"
        $EndDescr
        $Comp
        L Device:U U1
        U 1 1 66020001
        P 4200 2500
        F 0 "U1" H 4200 3300 50 0000 C CNN
        F 1 "nRF52840" H 4200 3200 50 0000 C CNN
        F 2 "Module:nRF52840" H 4200 2500 50 0001 C CNN
        F 3 "" H 4200 2500 50 0001 C CNN
        1 4200 2500
        1 0 0 -1
        $EndComp
        Text Notes 1300 6900 0 50 ~ 0
        I2C -> TMP117 + SHT45
SPI -> SX1262
GPIO -> reed switch + RGB LED
VBAT -> buck/boost 3V3
        $EndSCHEMATC
