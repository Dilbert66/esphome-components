#mongoose web server library
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import CORE,PLATFORM_ESP32


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
        }
    )
)
cv.only_on([PLATFORM_ESP32]),

async def to_code(config):
    if CORE.is_esp8266:  
        cg.add_build_flag("-DMG_IO_SIZE=512")
        cg.add_build_flag("-DMG_ENABLE_CUSTOM_MILLIS=1")  
        cg.add_build_flag("-DMG_ARCH=MG_ARCH_ESP8266")
        cg.add_library("ESP32Async/ESPAsyncTCP", "2.0.0")
    else:
        cg.add_build_flag("-DMG_ARCH=MG_ARCH_ESP32")
        cg.add_build_flag("-DMG_TLS=MG_TLS_MBED")
        cg.add_build_flag("-DMG_IO_SIZE=512")


