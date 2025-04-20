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

webdavbox_ns = cg.esphome_ns.namespace('webdavbox3')
WebDAVBox3 = webdavbox_ns.class_('WebDAVBox3', cg.Component)

# Schéma de configuration
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(WebDAVBox3),
    cv.GenerateID(web_server_base.CONF_WEB_SERVER_BASE_ID): cv.use_id(web_server_base.WebServerBase),
    cv.Optional(CONF_USERNAME, default=""): cv.string,  # Ajout de username
    cv.Optional(CONF_PASSWORD, default=""): cv.string,  # Ajout de password
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    base = await cg.get_variable(config[web_server_base.CONF_WEB_SERVER_BASE_ID])
    cg.add(var.set_base(base))

    # Si username est présent dans la configuration, l'ajouter à l'objet
    if CONF_USERNAME in config:
        cg.add(var.set_username(config[CONF_USERNAME]))  # Assignation de username
    
    # Si password est présent dans la configuration, l'ajouter à l'objet
    if CONF_PASSWORD in config:
        cg.add(var.set_password(config[CONF_PASSWORD]))  # Assignation de password











