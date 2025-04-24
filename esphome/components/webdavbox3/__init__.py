import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID, 
    CONF_USERNAME, 
    CONF_PASSWORD, 
    CONF_PORT
)
from .. import sd_mmc_card

CODEOWNERS = ["@youkorr"]
DEPENDENCIES = ["sd_mmc_card"]

# Define our own constant for root path
CONF_ROOT_PATH = "root_path"

MULTI_CONF = False

webdavbox_ns = cg.esphome_ns.namespace("webdavbox3")
WebDAVBox3 = webdavbox_ns.class_("WebDAVBox3", cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.Required(CONF_ID): cv.declare_id(WebDAVBox3),
    
    cv.GenerateID(sd_mmc_card.CONF_SD_MMC_CARD_ID): cv.use_id(sd_mmc_card.SdMmc),
    cv.Optional("url_prefix", default="/"): cv.string,
    cv.Optional(CONF_ROOT_PATH, default="/"): cv.string_strict,
    cv.Optional(CONF_PORT, default=8081): cv.port,
    cv.Optional(CONF_USERNAME, default=""): cv.string,
    cv.Optional(CONF_PASSWORD, default=""): cv.string,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    sdmmc = await cg.get_variable(config[sd_mmc_card.CONF_SD_MMC_CARD_ID])
    cg.add(var.set_sd_mmc_card(sdmmc))
    cg.add(var.set_url_prefix(config["url_prefix"]))
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_root_path(config[CONF_ROOT_PATH]))
    if CONF_USERNAME in config:
        cg.add(var.set_username(config[CONF_USERNAME]))
    if CONF_PASSWORD in config:
        cg.add(var.set_password(config[CONF_PASSWORD]))
    
    return var














