import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import web_server_base
from esphome.const import CONF_ID, CONF_USERNAME, CONF_PASSWORD

DEPENDENCIES = ['web_server', 'sdcard']
AUTO_LOAD = ['web_server']

webdav_ns = cg.esphome_ns.namespace('webdav')
WebDAVComponent = webdav_ns.class_('WebDAVComponent', cg.Component)

CONF_MOUNT_POINT = 'mount_point'
CONF_SD_CARD_ID = 'sd_card_id'

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(WebDAVComponent),
    cv.Required(CONF_SD_CARD_ID): cv.use_id(cg.SDCard),
    cv.Optional(CONF_MOUNT_POINT, default="/sdcard"): cv.string,
    cv.Optional(CONF_USERNAME, default=""): cv.string,
    cv.Optional(CONF_PASSWORD, default=""): cv.string,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    sd_card = await cg.get_variable(config[CONF_SD_CARD_ID])
    cg.add(var.set_sd_card(sd_card))
    cg.add(var.set_mount_point(config[CONF_MOUNT_POINT]))
    cg.add(var.set_username(config[CONF_USERNAME]))
    cg.add(var.set_password(config[CONF_PASSWORD]))

    # Add WebDAV handler to web server
    web_server = await cg.get_variable(config[CONF_ID])
    cg.add(web_server.add_handler(var))


