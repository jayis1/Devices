EESchema Schematic File Version 7
        EELAYER 0 0
        EELAYER END
        $Descr A4 11700 8268
        encoding utf-8
        Sheet 1 1
        Title "OutageSync Panel Controller"
        Date "2026-08-29"
        Rev "1.0"
        Comp "Devices"
        Comment1 "STM32G474 + ADE9153A + isolated RS-485"
        Comment2 "DIN rail power and contactor control"
        Comment3 "High-voltage sense / low-voltage control isolation"
        Comment4 "Branch priority management"
        $EndDescr
        $Comp
        L Device:U U1
        U 1 1 66010001
        P 4300 2500
        F 0 "U1" H 4300 3300 50 0000 C CNN
        F 1 "STM32G474RET6" H 4300 3200 50 0000 C CNN
        F 2 "Package_QFP:LQFP-64" H 4300 2500 50 0001 C CNN
        F 3 "" H 4300 2500 50 0001 C CNN
        1 4300 2500
        1 0 0 -1
        $EndComp
        $Comp
        L Device:U U2
        U 1 1 66010002
        P 7600 2500
        F 0 "U2" H 7600 3300 50 0000 C CNN
        F 1 "ADE9153A" H 7600 3200 50 0000 C CNN
        F 2 "Package_QFN:QFN-40" H 7600 2500 50 0001 C CNN
        F 3 "" H 7600 2500 50 0001 C CNN
        1 7600 2500
        1 0 0 -1
        $EndComp
        Text Notes 1400 7000 0 50 ~ 0
        CT inputs -> ADE9153A
SPI -> STM32G474
RS-485 -> inverter/ATS
GPIO -> K1..K4 contactors
24V DIN -> isolated 12V/5V/3V3 rails
        $EndSCHEMATC
