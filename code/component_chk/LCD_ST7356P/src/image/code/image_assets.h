#ifndef IMAGE_ASSETS_H
#define IMAGE_ASSETS_H

#include <stdint.h>
#include "image_asset_config.h"

typedef struct {
	uint16_t width;
	uint16_t height;
	const uint16_t *data;
	uint16_t delay;
} image_rgb565_t;

#ifndef IMAGE_ASSET_FRAME_DELAY_MS
#define IMAGE_ASSET_FRAME_DELAY_MS 50
#endif

#define IMAGE_GIF_SET_COUNT 4

#define IMAGE_ASSET_COUNT_FACE_IDLE_ALLAROUND 10
#define IMAGE_GIF_SET_FOLDER_FACE_IDLE_ALLAROUND "01_face_idle_allaround"
#define IMAGE_ASSET_COUNT_FACE_PRINTING_TRUCK 10
#define IMAGE_GIF_SET_FOLDER_FACE_PRINTING_TRUCK "02_face_printing_truck"
#define IMAGE_ASSET_COUNT_FACE_SLEEP 10
#define IMAGE_GIF_SET_FOLDER_FACE_SLEEP "03_face_sleep"
#define IMAGE_ASSET_COUNT_FACE_WAKEUP 10
#define IMAGE_GIF_SET_FOLDER_FACE_WAKEUP "04_face_wakeup"

extern const image_rgb565_t image_assets_face_idle_allaround[IMAGE_ASSET_COUNT_FACE_IDLE_ALLAROUND];
extern const image_rgb565_t image_assets_face_printing_truck[IMAGE_ASSET_COUNT_FACE_PRINTING_TRUCK];
extern const image_rgb565_t image_assets_face_sleep[IMAGE_ASSET_COUNT_FACE_SLEEP];
extern const image_rgb565_t image_assets_face_wakeup[IMAGE_ASSET_COUNT_FACE_WAKEUP];

typedef struct {
	const char *folder;
	uint16_t frame_count;
	const image_rgb565_t *frames;
} image_gif_set_t;

extern const image_gif_set_t image_gif_sets[IMAGE_GIF_SET_COUNT];

#endif
