set(COMPONENT_SRCS take_picture.c stram_server.c)
set(COMPONENT_ADD_INCLUDEDIRS .)
set(PRIV_REQUIRES esp32-camera nvs_flash protocol_examples_common esp_http_server esp-code-scanner)
register_component()