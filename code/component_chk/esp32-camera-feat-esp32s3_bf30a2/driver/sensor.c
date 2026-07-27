#include <stdio.h>
#include "sensor.h"

const camera_sensor_info_t camera_sensor[CAMERA_MODEL_MAX] = {
    {CAMERA_BF20A6, "BF20A6", BF20A6_SCCB_ADDR, BF20A6_PID, FRAMESIZE_VGA, false},
    {CAMERA_BF30A2, "BF30A2", BF30A2_SCCB_ADDR, BF30A2_PID, FRAMESIZE_QVGA, false},
};

const resolution_info_t resolution[FRAMESIZE_INVALID] = {
    {   96,   96, ASPECT_RATIO_1X1   }, /* 96x96 */
    {  160,  120, ASPECT_RATIO_4X3   }, /* QQVGA */
    {  176,  144, ASPECT_RATIO_5X4   }, /* QCIF  */
    {  240,  176, ASPECT_RATIO_4X3   }, /* HQVGA */
    {  200,  240, ASPECT_RATIO_1X1   }, /* 200x240 */
    {  240,  240, ASPECT_RATIO_1X1   }, /* 240x240 */

    // {  320,  240, ASPECT_RATIO_4X3   }, /* QVGA  */
    {  240,  320, ASPECT_RATIO_4X3   }, /* QVGA BF30A2  */

    {  400,  296, ASPECT_RATIO_4X3   }, /* CIF   */
    {  480,  320, ASPECT_RATIO_3X2   }, /* HVGA  */
    {  640,  480, ASPECT_RATIO_4X3   }, /* VGA   */
};

camera_sensor_info_t *esp_camera_sensor_get_info(sensor_id_t *id)
{
    for (int i = 0; i < CAMERA_MODEL_MAX; i++) {
        if (id->PID == camera_sensor[i].pid) {
            return (camera_sensor_info_t *)&camera_sensor[i];
        }
    }
    return NULL;
}
