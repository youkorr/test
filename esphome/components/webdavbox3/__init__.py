import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PORT, CONF_USERNAME, CONF_PASSWORD

CODEOWNERS = ["@your_username"]
DEPENDENCIES = ['web_server', 'sd_mmc']
MULTI_CONF = True

webdavbox3_ns = cg.esphome_ns.namespace('webdavbox3')
WebDAVBox3 = webdavbox3_ns.class_('WebDAVBox3', cg.Component)

CONFIG_SCHEMA = cv.All(
    cv.Schema({
        cv.Required(CONF_ID): cv.declare_id(WebDAVBox3),
        cv.Optional('root_path', default='/sdcard/'): cv.string,
        cv.Optional('url_prefix', default='/webdav'): cv.string,
        cv.Optional(CONF_PORT, default=8080): cv.port,
        cv.Optional(CONF_USERNAME): cv.string,
        cv.Optional(CONF_PASSWORD): cv.string,
    }).extend(cv.COMPONENT_SCHEMA),
    cv.has_at_least_one_key(CONF_USERNAME, CONF_PASSWORD),
)

def validate_auth(config):
    if (CONF_USERNAME in config) != (CONF_PASSWORD in config):
        raise cv.Invalid("Both username and password must be specified together")
    return config

FINAL_VALIDATE_SCHEMA = cv.All(
    CONFIG_SCHEMA,
    validate_auth
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    cg.add(var.set_root_path(config['root_path']))
    cg.add(var.set_url_prefix(config['url_prefix']))
    cg.add(var.set_port(config[CONF_PORT]))
    
    if CONF_USERNAME in config:
        cg.add(var.set_username(config[CONF_USERNAME]))
    if CONF_PASSWORD in config:
        cg.add(var.set_password(config[CONF_PASSWORD]))
    
    return var


