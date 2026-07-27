# ESP32-S3 SPI Camera Driver

## General Information

This repository hosts ESP32 series Soc compatible driver for image sensors. Additionally it provides a few tools, which allow converting the captured frame data to the more common BMP and JPEG formats.

### Supported Soc

- ESP32-S3

### Supported Sensor

| model   | max resolution | color type | output format                                                | Len Size |
| ------- | -------------- | ---------- | ------------------------------------------------------------ | -------- |
| BF30A2  | 240 x 320      | color      | YCbCr422, Only Y                                             | 1/15"    |

## Important to Remember

- Please use a camera with SPI interface under the current framework. The camera sensor must work on 1-bit-SPI mode.

- Note that this interface does not follow the standard SPI protocol, and the SPI CS cable of the development board must be connected to another GPIO for controlling SPI transaction. Please change `SPI_CS_CTAL_PIN` in `esp_camera.c`.

- Currently an image is received over multiple DMA transactions. The following conditions need to be followed.

  - Image size is an integer multiple of 4 bytes
  - Image size is an integer multiple of the DMA transfer size
  - DMA transfer size is an integer multiple of 4 bytes
  - DMA transfer size ≤ 4092 bytes

    Users need to configure the image size(including line header) `TOTAL_SIZE`, DMA transfer size `DMA_TRANS_LEN` and number of DMA descriptors `QUEUE_SIZE` in `esp_camera.c`.

    For example, for a 240*320 only Y image, total = 80640 bytes = 320 \* (12 + 240) bytes = 21 transfer times \* 3840 bytes transfer size = 20 transfer times \* 4032 bytes transfer size. Example configuration can be `TOTAL_SIZE = 80640, DMA_TRANS_LEN = 3840, QUEUE_SIZE = 21`.

    Tool `Calculate_DMA_length.py` can be used to calculate the appropriate DMA transfer size, the input parameter is the width and height of the image. Usage `python Calculate_DMA_length.py -w 240 -h 320`.

    If the data receiving cannot be continuous, please try to increase number of DMA descriptors `QUEUE_SIZE`.

- The camera is currently configured to output Only Y images with a 12-byte line header, and the image buffer contains the line header. For example, for a 240*320 image, buffer size = 320 \* (12 + 240) bytes

- In the current implementation, there is no frame synchronization signal, only by controlling the power down function of the sensor to obtain data from the frame header. Therefore, after turning on power on, please use the Logic analyzer or oscilloscope to determine the delay time from power on to the official output data. Later, you can explore using a data output format containing frame synchronization codes to enhance the fault tolerance of the driver. Or just use pwdn pin to implement frame synchronization mechanism.

## Related APIs

```c
esp_err_t esp_spi_cam_init(const camera_config_t *config);
camera_fb_t *esp_spi_cam_take(TickType_t timeout);
void esp_spi_cam_give(camera_fb_t *cam_frame);
sensor_t *esp_spi_cam_sensor_get();
esp_err_t esp_spi_cam_deinit(void);
```

### Using esp-idf

- Clone or download and extract the repository to the components folder of your ESP-IDF project
- Enable PSRAM in `menuconfig` (also set Flash and PSRAM frequiencies to 80MHz)
- Include `esp_camera.h` in your code
- If using ESP32-S3 on IDF 4.4 version, you needs to merge `20240319_spi_camera.patch` into IDF. The merge command is as follows:

```bash
cd $IDF_PATH
git apply "This project path"/20240319_spi_camera.patch
```
