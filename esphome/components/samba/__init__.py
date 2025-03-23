import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.components import sd_mmc_card

DEPENDENCIES = ['esp32']

samba_ns = cg.esphome_ns.namespace('samba_server')
SambaServer = samba_ns.class_('SambaServer', cg.Component)

CONF_WORKGROUP = 'workgroup'
CONF_SHARE_NAME = 'share_name'
CONF_USERNAME = 'username'
CONF_PASSWORD = 'password'
CONF_ROOT_PATH = 'root_path'
CONF_SD_MMC_CARD = 'sd_mmc_card'

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(SambaServer),
    cv.Optional(CONF_WORKGROUP, default='WORKGROUP'): cv.string,
    cv.Optional(CONF_SHARE_NAME, default='ESP32'): cv.string,
    cv.Optional(CONF_USERNAME, default='esp32'): cv.string,
    cv.Optional(CONF_PASSWORD, default='password'): cv.string,
    cv.Optional(CONF_ROOT_PATH, default='/sdcard'): cv.string,
    cv.Required(CONF_SD_MMC_CARD): cv.use_id(sd_mmc_card.SdMmc),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    cg.add(var.set_workgroup(config[CONF_WORKGROUP]))
    cg.add(var.set_share_name(config[CONF_SHARE_NAME]))
    cg.add(var.set_username(config[CONF_USERNAME]))
    cg.add(var.set_password(config[CONF_PASSWORD]))
    cg.add(var.set_root_path(config[CONF_ROOT_PATH]))
    
    sd_card = await cg.get_variable(config[CONF_SD_MMC_CARD])
    cg.add(var.set_sd_mmc_card(sd_card))





