import esphome.config_validation as cv
from esphome.components import sdcard
from esphome.const import CONF_ID, CONF_MOUNT_POINT, CONF_USERNAME, CONF_PASSWORD, CONF_SD_CARD
from esphome.core import HexInt
from esphome.cpp_generator import Pvariable, get_variable
from esphome.cpp_types import App

DEPENDENCIES = ['network', 'sdcard']

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(webdav::WebDAV),
    cv.Required(CONF_SD_CARD): cv.use_id(sdcard.SDCardComponent),
    cv.Required(CONF_MOUNT_POINT): cv.string_strict,
    cv.Optional(CONF_USERNAME, default=""): cv.string_strict,
    cv.Optional(CONF_PASSWORD, default=""): cv.string_strict,
}).extend(cv.COMPONENT_SCHEMA)

def to_code(config):
    sd_card = yield get_variable(config[CONF_SD_CARD])
    rhs = webdav::WebDAV::create(sd_card, config[CONF_MOUNT_POINT], config[CONF_USERNAME], config[CONF_PASSWORD])
    webdav = Pvariable(config[CONF_ID], rhs)
    yield

