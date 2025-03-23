import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import web_server_base, sd_mmc_card
from esphome.const import CONF_ID

DEPENDENCIES = ['web_server_base']
AUTO_LOAD = ['sd_mmc_card']

samba_ns = cg.esphome_ns.namespace('samba')
SambaComponent = samba_ns.class_('SambaComponent', cg.Component)

CONF_WEB_SERVER_BASE_ID = 'web_server_base_id'
CONF_SD_MMC_CARD_ID = 'sd_mmc_card'
CONF_SHARE_NAME = 'share_name'
CONF_ROOT_PATH = 'root_path'

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(SambaComponent),
    cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(web_server_base.WebServerBase),
    cv.Required(CONF_SD_MMC_CARD_ID): cv.use_id(sd_mmc_card.SdMmc),
    cv.Optional(CONF_SHARE_NAME, default='ESP32'): cv.string,
    cv.Optional(CONF_ROOT_PATH, default='/sdcard'): cv.string,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    web_server = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])
    cg.add(var.set_web_server(web_server))
    
    sd_card = await cg.get_variable(config[CONF_SD_MMC_CARD_ID])
    cg.add(var.set_sd_mmc_card(sd_card))
    
    cg.add(var.set_share_name(config[CONF_SHARE_NAME]))
    cg.add(var.set_root_path(config[CONF_ROOT_PATH]))
