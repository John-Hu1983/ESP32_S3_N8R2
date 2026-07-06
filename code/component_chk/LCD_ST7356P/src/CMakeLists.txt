# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

idf_component_register(SRCS "main.c"
							"dev/lcd_st7365p.c"
							"dev/lvgl_port.c"
							"image/code/image_assets.c"
					   INCLUDE_DIRS "." "dev" "image/code"
					   REQUIRES esp_driver_gpio esp_driver_spi heap freertos log esp_timer lvgl)

target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_CONF_INCLUDE_SIMPLE)
