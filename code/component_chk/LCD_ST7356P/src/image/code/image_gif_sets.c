#include "image_assets.h"

const image_gif_set_t image_gif_sets[IMAGE_GIF_SET_COUNT] = {
	{ IMAGE_GIF_SET_FOLDER_FACE_WAKEUP, IMAGE_ASSET_COUNT_FACE_WAKEUP, image_assets_face_wakeup },
	{ IMAGE_GIF_SET_FOLDER_FACE_IDLE_ALLAROUND, IMAGE_ASSET_COUNT_FACE_IDLE_ALLAROUND, image_assets_face_idle_allaround },
	{ IMAGE_GIF_SET_FOLDER_FACE_PRINTING_TRUCK, IMAGE_ASSET_COUNT_FACE_PRINTING_TRUCK, image_assets_face_printing_truck },
	{ IMAGE_GIF_SET_FOLDER_FACE_SLEEP, IMAGE_ASSET_COUNT_FACE_SLEEP, image_assets_face_sleep },
};
