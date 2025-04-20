import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_USERNAME,
    CONF_PASSWORD,
)
from esphome.components import web_server_base

DEPENDENCIES = ['web_server_base', 'sd_mmc_card']
MULTI_CONF = False

webdavbox_ns = cg.esphome_ns.namespace('webdavbox3')  # Utilisation de webdavbox3
WebDavServer = webdavbox_ns.class_('WebDavServer', cg.Component)

# Schéma de configuration, avec validation de username et password
CONFIG_SCHEMA = cv.Schema({
    cv.Required(CONF_ID): cv.declare_id(WebDavServer),
    cv.Required('root_path'): cv.string,
    cv.Required('url_prefix'): cv.string,
    cv.Required('port'): cv.port,
    cv.Optional(CONF_USERNAME, default=""): cv.string,
    cv.Optional(CONF_PASSWORD, default=""): cv.string,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Configurer les chemins et autres options
    cg.add(var.set_root_path(config['root_path']))
    cg.add(var.set_url_prefix(config['url_prefix']))
    cg.add(var.set_port(config['port']))

    # Ajouter username et password si présents
    if CONF_USERNAME in config:
        cg.add(var.set_username(config[CONF_USERNAME]))
    
    if CONF_PASSWORD in config:
        cg.add(var.set_password(config[CONF_PASSWORD]))

    return var









