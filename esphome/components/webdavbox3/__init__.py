import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_USERNAME, CONF_PASSWORD, CONF_PORT

DEPENDENCIES = ["sd_mmc_card"]
AUTO_LOAD = ["sd_mmc_card"]

CONF_SD_CARD_ID = "sd_card_id"

webdavbox_ns = cg.esphome_ns.namespace("webdavbox3")
WebDAVBox3 = webdavbox_ns.class_("WebDAVBox3", cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(WebDAVBox3),
    cv.Required(CONF_SD_CARD_ID): cv.use_id(cg.Component),
    cv.Optional("root_path", default="/sdcard"): cv.string,
    cv.Optional("url_prefix", default="/"): cv.string,
    cv.Optional(CONF_PORT, default=8081): cv.port,
    cv.Optional(CONF_USERNAME, default=""): cv.string,
    cv.Optional(CONF_PASSWORD, default=""): cv.string,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    sd = await cg.get_variable(config[CONF_SD_CARD_ID])
    cg.add(var.set_sd_card(sd))
    
    cg.add(var.set_root_path(config["root_path"]))
    cg.add(var.set_url_prefix(config["url_prefix"]))
    cg.add(var.set_port(config[CONF_PORT]))
    
    if CONF_USERNAME in config:
        cg.add(var.set_username(config[CONF_USERNAME]))
    if CONF_PASSWORD in config:
        cg.add(var.set_password(config[CONF_PASSWORD]))













