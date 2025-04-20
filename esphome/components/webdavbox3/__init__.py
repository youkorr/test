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

webdavbox_ns = cg.esphome_ns.namespace('webdavbox')
WebDavServer = webdavbox_ns.class_('WebDavServer', cg.Component)

# Schéma de configuration avec validation pour username et password
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(WebDavServer),
    cv.GenerateID(web_server_base.CONF_WEB_SERVER_BASE_ID): cv.use_id(web_server_base.WebServerBase),
    cv.Optional(CONF_USERNAME, default=""): cv.string,
    cv.Optional(CONF_PASSWORD, default=""): cv.string,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Récupérer la base du serveur web
    base = await cg.get_variable(config[web_server_base.CONF_WEB_SERVER_BASE_ID])
    cg.add(var.set_base(base))

    # Si username est défini dans la configuration, l'ajouter
    if CONF_USERNAME in config:
        cg.add(var.set_username(config[CONF_USERNAME]))
    
    # Si password est défini dans la configuration, l'ajouter
    if CONF_PASSWORD in config:
        cg.add(var.set_password(config[CONF_PASSWORD]))

    return var








