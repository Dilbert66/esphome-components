#mongoose web server library
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import CORE


KEY_BOARD = "board"
KEY_RP2040 = "rp2040"

async def to_code(config):
    if CORE.is_esp8266:  
        cg.add_build_flag("-DMG_ARCH=MG_ARCH_CUSTOM")
        cg.add_library("ESP32Async/ESPAsyncTCP", "2.0.0")
    elif CORE.is_esp32:
        cg.add_build_flag("-DMG_ARCH=MG_ARCH_ESP32")
        cg.add_build_flag("-DMG_TLS=MG_TLS_MBED")
        cg.add_build_flag("-DMG_IO_SIZE=512")
    elif CORE.is_rp2040:
        cg.add_build_flag("-DMG_ARCH=MG_ARCH_CUSTOM")
        if CORE.data[KEY_RP2040][KEY_BOARD] == "rpipicow":
            cg.add_library("khoih-prog/AsyncTCP_RP2040W", "1.2.0")



