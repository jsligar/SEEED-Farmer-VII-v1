idf_component_register(SRCS "main.c" 
                    INCLUDE_DIRS "."
                    REQUIRES sx1262 board_hal_seeed nvs_flash console)
