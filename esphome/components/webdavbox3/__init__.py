import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PORT

CODEOWNERS = ["@your_username"]
DEPENDENCIES = ['web_server', 'sd_mmc_card']
MULTI_CONF = True

webdavbox3_ns = cg.esphome_ns.namespace('webdavbox3')
WebDAVBox3 = webdavbox3_ns.class_('WebDAVBox3')

CONFIG_SCHEMA = cv.Schema({
    cv.Required(CONF_ID): cv.declare_id(WebDAVBox3),
    cv.Optional('root_path', default='/sdcard/'): cv.string,
    cv.Optional('url_prefix', default='/webdav'): cv.string,
    cv.Optional(CONF_PORT, default=8080): cv.port,
})

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    
    cg.add(var.set_root_path(config['root_path']))
    cg.add(var.set_url_prefix(config['url_prefix']))
    cg.add(var.set_port(config[CONF_PORT]))
    
    return var


