Import("env")

# Ensure LittleFS partition is used for data upload
env.Replace(
    BOARD_FLASH_ERASE_ALL=True,
    UPLOAD_PROTOCOL="esptool",
    UPLOAD_FLAGS=[
        "--chip", "esp32",
        "--before", "default_reset",
        "--after", "hard_reset",
    ]
)
