#ifndef IMAGE_ASSETS_H
#define IMAGE_ASSETS_H

#include <stdint.h>
#include "image_asset_config.h"

typedef struct
{
	uint16_t width;
	uint16_t height;
	const uint16_t *data;
	uint16_t delay;
} image_rgb565_t;

#define DRAW_IMAGE_PERIOD 83

#define IMAGE_GIF_SET_COUNT 4

#define IMAGE_ASSET_COUNT_1 10
#define IMAGE_GIF_SET_FOLDER_1 "01_face_idle_allaround"
#define IMAGE_ASSET_COUNT_2 10
#define IMAGE_GIF_SET_FOLDER_2 "02_face_printing_truck"
#define IMAGE_ASSET_COUNT_3 10
#define IMAGE_GIF_SET_FOLDER_3 "03_face_sleep"
#define IMAGE_ASSET_COUNT_4 10
#define IMAGE_GIF_SET_FOLDER_4 "04_face_wakeup"

extern const image_rgb565_t image_assets_1[IMAGE_ASSET_COUNT_1];
extern const image_rgb565_t image_face_sleep[IMAGE_ASSET_COUNT_2];
extern const image_rgb565_t image_face_printing[IMAGE_ASSET_COUNT_3];
extern const image_rgb565_t image_assets_4[IMAGE_ASSET_COUNT_4];

typedef struct
{
	const char *folder;
	uint16_t frame_count;
	const image_rgb565_t *frames;
} image_gif_set_t;

extern const image_gif_set_t image_gif_sets[IMAGE_GIF_SET_COUNT];

#endif
