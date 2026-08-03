idf_component_register(
    SRCS "diag.c"
    INCLUDE_DIRS "include"
    REQUIRES
        esp_app_format      # esp_app_get_description() — version / build stamp
        esp_hw_support      # esp_chip_info, esp_read_mac
        spi_flash           # esp_flash_get_size
        esp_timer           # uptime
        esp_wifi            # associated AP record (SSID + RSSI)
        network             # wifi_get_ip
        sdcard              # mount state + SD_MOUNT_POINT
        fatfs               # esp_vfs_fat_info — free/total on the mounted card
        updater             # variant key + update / www staleness flags
)
