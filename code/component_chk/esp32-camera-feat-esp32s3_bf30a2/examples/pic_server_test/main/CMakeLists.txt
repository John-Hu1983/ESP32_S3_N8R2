set(COMPONENT_SRCS take_picture.c pic_server.c)
set(COMPONENT_ADD_INCLUDEDIRS .)
set(PRIV_REQUIRES nvs_flash protocol_examples_common esp_http_server)
register_component()