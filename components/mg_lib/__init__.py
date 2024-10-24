import esphome.config_validation as cv
import os
import pathlib
from esphome.helpers import copy_file_if_changed
from esphome.core import CORE


CONFIG_SCHEMA = cv.Schema({})


async def to_code(config):
    copy_file_if_changed(
       os.path.join(pathlib.Path(__file__).parent.resolve(),"mongoose.h"),
       CORE.relative_build_path("src/mongoose.h"),
    )
