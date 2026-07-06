#ifndef IMAGE_ASSETS_H
#define IMAGE_ASSETS_H

#include <stdint.h>

typedef struct {
	uint16_t width;
	uint16_t height;
	const uint16_t *data;
} image_rgb565_t;

#define IMAGE_ASSET_COUNT 20

extern const image_rgb565_t image_assets[IMAGE_ASSET_COUNT];

#endif
