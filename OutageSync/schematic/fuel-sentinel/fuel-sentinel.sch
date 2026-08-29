EESchema Schematic File Version 7
        EELAYER 0 0
        EELAYER END
        $Descr A4 11700 8268
        encoding utf-8
        Sheet 1 1
        Title "OutageSync Fuel & Air Sentinel"
        Date "2026-08-29"
        Rev "1.0"
        Comp "Devices"
        Comment1 "ESP32-S3 + SX1262 + SCD41 + MICS-6814"
        Comment2 "Generator enclosure safety and fuel telemetry"
        Comment3 "12V battery input with dry-contact inhibit"
        Comment4 "CO/CO2/heat/vibration/fuel"
        $EndDescr
        $Comp
        L Device:U U1
        U 1 1 66040001
        P 4300 2500
        F 0 "U1" H 4300 3300 50 0000 C CNN
        F 1 "ESP32-S3-WROOM-1" H 4300 3200 50 0000 C CNN
        F 2 "RF_Module:ESP32-S3" H 4300 2500 50 0001 C CNN
        F 3 "" H 4300 2500 50 0001 C CNN
        1 4300 2500
        1 0 0 -1
        $EndComp
        Text Notes 1300 6900 0 50 ~ 0
        I2C -> SCD41 + accel
ADC -> MICS-6814 front-end + TMP235
UART/SPI -> SX1262
GPIO -> inhibit relay + strobe
        $EndSCHEMATC
