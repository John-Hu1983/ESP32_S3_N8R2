Import("env")

# Allow ESP-IDF to proceed with a non-default toolchain version.
env["ENV"]["IDF_MAINTAINER"] = "1"
