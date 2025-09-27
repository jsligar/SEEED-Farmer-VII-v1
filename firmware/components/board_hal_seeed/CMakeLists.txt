idf_component_register(SRCS "board_hal_seeed.c" "oled.c" "rtc.c" "sd.c" "buzzer.c" "button.c" "servo.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver)