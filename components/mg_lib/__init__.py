#mongoose web server library
import esphome.codegen as cg


async def to_code(config):
    cg.add_build_flag("-DMG_TLS=MG_TLS_MBED")
    #cg.add_build_flag("-DMG_TLS=MG_TLS_BUILTIN")
    cg.add_build_flag("-DMG_IO_SIZE=512")
    # cg.add_build_flag("-DMG_ENABLE_POLL")

