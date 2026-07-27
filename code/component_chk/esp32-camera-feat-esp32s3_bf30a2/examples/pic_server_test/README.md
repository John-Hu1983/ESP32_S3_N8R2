| Supported Targets | ESP32 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- |

# ESP Camera Picture Server

The example starts a web server on a local network. You can use a browser to access this server to view pictures.  

## How to use the example


### Hardware Required

* A development board with camera module (e.g., ESP32-S3, etc.)
* A USB cable for power supply and programming
* SPI connect:

| sensor    | ESP32S3（SPI CS 管脚: 默认 pin 10 接地） |      |
| --------- | ---------------------------------------- | ---- |
| SDA       | 8 参考init_camera()                      |      |
| SCL       | 9参考init_camera()                       |      |
| PWDN      | NC                                       |      |
| D1        | NC                                       |      |
| MCLK      | 4参考init_camera()                       |      |
| GND       | GND                                      |      |
| PCLK      | GPIO_SCLK：12                            |      |
| D0        | GPIO_MOSI： 11                           |      |
| AVDD2.8v  |                                          |      |
| IOVDD2.8v |                                          |      |

### Configure the project

Configure your wifi.

```
idf.py menuconfig -> Example Connection Configuration
```

Launch and monitor
Flash the program and launch IDF Monitor:

```bash
idf.py flash monitor
```

Test the example interactively on a web browser (assuming IP is 192.168.43.130):

open path `http://192.168.43.130/pic` to see an HTML web page on the server.

Click the refresh button to get next picture on the HTML web page.
