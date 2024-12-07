import esphome.config_validation as cv
import os
import pathlib
from esphome.helpers import copy_file_if_changed
from esphome.core import CORE,coroutine_with_priority
import logging
_LOGGER = logging.getLogger(__name__)
CONFIG_SCHEMA = cv.Schema({})

@coroutine_with_priority(40.0)
async def to_code(config):
    src=os.path.join(pathlib.Path(__file__).parent.resolve(),"mongoose.h_h")
    dst=CORE.relative_build_path("src/mongoose.h")
    if os.path.isfile(src) and not os.path.isfile(dst):
      copy_file_if_changed(src,dst)
      _LOGGER.info("Copied mongoose.h")
    src=os.path.join(pathlib.Path(__file__).parent.resolve(),"mongoose.c_c")
    dst=CORE.relative_build_path("src/mongoose.c")
    if os.path.isfile(src) and not os.path.isfile(dst):
      copy_file_if_changed(src,dst)
      _LOGGER.info("Copied mongoose.c")


