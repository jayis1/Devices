EESchema Schematic File Version 7
        EELAYER 0 0
        EELAYER END
        $Descr A4 11700 8268
        encoding utf-8
        Sheet 1 1
        Title "OutageSync Critical Outlet Node"
        Date "2026-08-29"
        Rev "1.0"
        Comp "Devices"
        Comment1 "ESP32-C6 + ATM90E26 + latching relay"
        Comment2 "AC mains metering and priority shedding"
        Comment3 "Isolated PSU and weld detection"
        Comment4 "Manual override button"
        $EndDescr
        $Comp
        L Device:U U1
        U 1 1 66030001
        P 4300 2500
        F 0 "U1" H 4300 3300 50 0000 C CNN
        F 1 "ESP32-C6-MINI-1" H 4300 3200 50 0000 C CNN
        F 2 "RF_Module:ESP32-C6" H 4300 2500 50 0001 C CNN
        F 3 "" H 4300 2500 50 0001 C CNN
        1 4300 2500
        1 0 0 -1
        $EndComp
        Text Notes 1400 7000 0 50 ~ 0
        SPI -> ATM90E26
GPIO -> latching relay driver
GPIO -> override button + LED
AC in -> isolated 5V/3V3 PSU
        $EndSCHEMATC
