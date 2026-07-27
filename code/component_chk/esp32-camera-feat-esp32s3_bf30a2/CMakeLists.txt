# get IDF version for comparison
set(idf_version "${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}")

# set conversion sources
set(COMPONENT_SRCDIRS ./conversions)

set(COMPONENT_PRIV_INCLUDEDIRS ./conversions/private_include)

set(COMPONENT_ADD_INCLUDEDIRS ./driver/include
                              ./conversions/include)

# set(COMPONENT_REQUIRES driver)

# # set driver sources only for supported platforms
if((IDF_TARGET STREQUAL "esp32s3") OR (IDF_TARGET STREQUAL "esp32c3"))
  list(APPEND COMPONENT_SRCDIRS ./driver
                                ./sensors
                                ./target)

  list(APPEND COMPONENT_PRIV_INCLUDEDIRS ./driver/private_include
                                         ./sensors/private_include
                                         ./target/private_include)

  set(COMPONENT_PRIV_REQUIRES freertos nvs_flash)

  if (IDF_VERSION_MAJOR GREATER_EQUAL 5)
    list(APPEND COMPONENT_PRIV_REQUIRES
         driver
         esp_driver_gpio
         esp_driver_i2c
         esp_driver_ledc
         esp_driver_spi)
  else()
    list(APPEND COMPONENT_PRIV_REQUIRES driver)
  endif()

  set(min_version_for_esp_timer "4.2")

  if (idf_version VERSION_GREATER_EQUAL min_version_for_esp_timer)
    list(APPEND COMPONENT_PRIV_REQUIRES esp_timer)
  endif()

else()  # NOT support target
  message(FATAL_ERROR "Current Target not support spi camera.")

endif()

register_component()
