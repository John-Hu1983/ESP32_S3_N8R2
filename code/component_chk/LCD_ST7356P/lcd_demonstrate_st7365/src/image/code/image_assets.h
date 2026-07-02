#ifndef IMAGE_ASSETS_H
#define IMAGE_ASSETS_H

#include <stdint.h>

#include "image1.h"
#include "image2.h"
#include "image3.h"
#include "image4.h"
#include "image5.h"

typedef struct {
	uint16_t width;
	uint16_t height;
	const uint16_t *data;
} image_rgb565_t;

#define IMAGE_ASSET_COUNT 5

extern const image_rgb565_t image_assets[IMAGE_ASSET_COUNT];

#endif
