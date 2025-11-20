#mongoose web server library
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import CORE


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
        }
    )
)

async def to_code(config):
    if CORE.is_esp8266:  
        cg.add_build_flag("-DMG_IO_SIZE=512")
        cg.add_build_flag("-DMG_ENABLE_CUSTOM_MILLIS=1")  
        cg.add_build_flag("-DMG_ARCH=MG_ARCH_ESP8266")
        cg.add_build_flag("-DLWIP_TIMEVAL_PRIVATE=0")
        #cg.add_build_flag("-DMG_ENABLE_LWIP=1")
        #cg.add_build_flag("-DLWIP_SOCKET=1")
        #cg.add_build_flag("-DLWIP_POSIX_SOCKETS_IO_NAMES=1")
        #cg.add_build_flag("-DLWIP_COMPAT_SOCKETS=1")
        #cg.add_build_flag("-DMG_TLS=MG_TLS_MBED")
    else:
        cg.add_build_flag("-DMG_ARCH=MG_ARCH_ESP32")
        cg.add_build_flag("-DMG_TLS=MG_TLS_MBED")
        #cg.add_build_flag("-DMG_ENABLE_CUSTOM_MILLIS=1")  
        #cg.add_build_flag("-DMG_TLS=MG_TLS_BUILTIN")
        cg.add_build_flag("-DMG_IO_SIZE=512")


