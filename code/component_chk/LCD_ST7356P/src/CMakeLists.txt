# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

idf_component_register(SRCS "main.c"
							"dev/lcd_st7365p.c"
							"dev/lvgl_port.c"
							"image/code/image1.c"
							"image/code/image2.c"
							"image/code/image3.c"
							"image/code/image4.c"
							"image/code/image5.c"
							"image/code/image_assets.c"
					   INCLUDE_DIRS "." "dev" "image/code"
					   REQUIRES esp_driver_gpio esp_driver_spi heap freertos log esp_timer lvgl)

target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_CONF_INCLUDE_SIMPLE)
