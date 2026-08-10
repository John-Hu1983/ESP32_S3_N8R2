# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

set(IMAGE_SCRIPT_DIR ${CMAKE_CURRENT_LIST_DIR}/image)
set(EXTRACT_GIF_SCRIPT ${IMAGE_SCRIPT_DIR}/extract_gif_frames.py)
set(CONVERT_IMAGE_SCRIPT ${IMAGE_SCRIPT_DIR}/convert_images_to_rgb565.py)
set(IMAGE_CONFIG_HEADER ${IMAGE_SCRIPT_DIR}/code/image_asset_config.h)
set(IMAGE_MANIFEST ${IMAGE_SCRIPT_DIR}/source/gif_sets/manifest.json)
set(IMAGE_ASSETS_H ${IMAGE_SCRIPT_DIR}/code/image_assets.h)
set(IMAGE_ASSETS_1_C ${IMAGE_SCRIPT_DIR}/code/image_assets_1.c)
set(IMAGE_ASSETS_2_C ${IMAGE_SCRIPT_DIR}/code/image_assets_2.c)
set(IMAGE_ASSETS_3_C ${IMAGE_SCRIPT_DIR}/code/image_assets_3.c)
set(IMAGE_ASSETS_4_C ${IMAGE_SCRIPT_DIR}/code/image_assets_4.c)
set(IMAGE_GIF_SETS_C ${IMAGE_SCRIPT_DIR}/code/image_gif_sets.c)
set(IMAGE_ASSET_SOURCE_FILES
	${IMAGE_ASSETS_1_C}
	${IMAGE_ASSETS_2_C}
	${IMAGE_ASSETS_3_C}
	${IMAGE_ASSETS_4_C}
	${IMAGE_GIF_SETS_C}
)
file(GLOB GIF_SOURCE_FILES ${CMAKE_CURRENT_LIST_DIR}/../rsc/gif/*.gif)

if(NOT CMAKE_SCRIPT_MODE_FILE)
	add_custom_command(
		OUTPUT ${IMAGE_MANIFEST}
		COMMAND ${PYTHON} ${EXTRACT_GIF_SCRIPT}
		WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
		DEPENDS ${EXTRACT_GIF_SCRIPT} ${IMAGE_CONFIG_HEADER} ${GIF_SOURCE_FILES}
		COMMENT "Extract GIF frames to 480x320 PNG sets"
		VERBATIM
	)

	add_custom_command(
		OUTPUT ${IMAGE_ASSET_SOURCE_FILES} ${IMAGE_ASSETS_H}
		COMMAND ${PYTHON} ${CONVERT_IMAGE_SCRIPT}
		WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
		DEPENDS ${CONVERT_IMAGE_SCRIPT} ${IMAGE_CONFIG_HEADER} ${IMAGE_MANIFEST}
		COMMENT "Generate RGB565 image asset source files"
		VERBATIM
	)

	add_custom_target(generate_image_assets ALL
		DEPENDS ${IMAGE_ASSET_SOURCE_FILES} ${IMAGE_ASSETS_H}
	)
endif()

idf_component_register(SRCS "main.c"
							"dev/lcd_st7365p.c"
							"dev/lvgl_port.c"
							${IMAGE_ASSET_SOURCE_FILES}
					   INCLUDE_DIRS "." "dev" "image/code"
					   REQUIRES esp_driver_gpio esp_driver_spi heap freertos log esp_timer lvgl)

if(TARGET generate_image_assets)
	add_dependencies(${COMPONENT_LIB} generate_image_assets)
endif()
target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_CONF_INCLUDE_SIMPLE)
