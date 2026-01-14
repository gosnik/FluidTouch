cmake_minimum_required(VERSION 3.16.0)

if(IDF_TARGET STREQUAL "esp32p4")
    # ESP-ADF components use legacy FreeRTOS types not supported on ESP32-P4.
    list(APPEND EXTRA_COMPONENT_DIRS
        "${CMAKE_CURRENT_LIST_DIR}/lib/esp-bsp/bsp/esp32_p4_function_ev_board"
    )
    list(APPEND EXCLUDE_COMPONENTS
        adf_utils
        audio_board
        audio_hal
        audio_mixer
        audio_pipeline
        audio_recorder
        audio_sal
        audio_stream
        battery_service
        bluetooth_service
        coredump_upload_service
        display_service
        dueros_service
        esp-adf-libs
        esp_actions
        esp_dispatcher
        esp_event_cast
        esp_peripherals
        es7210
        es8311
        fbm320
        input_key_service
        hts221
        mag3110
        mpu6050
        ota_service
        playlist
        tone_partition
        wifi_service
    )
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(FluidTouch)
