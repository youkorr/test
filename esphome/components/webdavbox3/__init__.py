import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@your_username"]
DEPENDENCIES = ['web_server', 'sd_mmc_card']
MULTI_CONF = True

webdavbox3_ns = cg.esphome_ns.namespace('webdavbox3')
WebDAVBox3 = webdavbox3_ns.class_('WebDAVBox3')

CONFIG_SCHEMA = cv.Schema({
    cv.Required('id'): cv.declare_id(WebDAVBox3),
    cv.Optional('root_path', default='/sdcard/'): cv.string,
})

async def to_code(config):
    # Créer une nouvelle instance avec l'ID fourni
    var = cg.new_Pvariable(config[CONF_ID])
    
    if 'root_path' in config:
        cg.add(var.set_root_path(config['root_path']))
    
    return var


