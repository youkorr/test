import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import web_server_base
from esphome.const import CONF_ID, CONF_USERNAME, CONF_PASSWORD

DEPENDENCIES = ['web_server_base']
AUTO_LOAD = ['web_server_base']

CONF_SD_CARD_ID = 'sd_card_id'
CONF_MOUNT_POINT = 'mount_point'

sd_webdav_ns = cg.esphome_ns.namespace('sd_webdav')
SDWebDAVComponent = sd_webdav_ns.class_('SDWebDAVComponent', cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(SDWebDAVComponent),
    cv.Required(CONF_SD_CARD_ID): cv.use_id(cg.esphome_ns.SDCard),
    cv.Required(CONF_MOUNT_POINT): cv.string,
    cv.Optional(CONF_USERNAME, default=''): cv.string,
    cv.Optional(CONF_PASSWORD, default=''): cv.string,
}).extend(cv.COMPONENT_SCHEMA)

def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    
    sd_card = yield cg.get_variable(config[CONF_SD_CARD_ID])
    cg.add(var.set_sd_card(sd_card))
    
    cg.add(var.set_mount_point(config[CONF_MOUNT_POINT]))
    cg.add(var.set_credentials(
        config[CONF_USERNAME],
        config[CONF_PASSWORD]
    ))
