#mongoose web server library
import esphome.codegen as cg
import esphome.config_validation as cv

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
        }
    )
)

async def to_code(config):
    cg.add_build_flag("-DMG_ARCH=MG_ARCH_ESP32")
    cg.add_build_flag("-DMG_TLS=MG_TLS_MBED")
    #cg.add_build_flag("-DMG_TLS=MG_TLS_BUILTIN")
    cg.add_build_flag("-DMG_IO_SIZE=512")


