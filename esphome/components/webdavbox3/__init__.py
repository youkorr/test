import esphome.codegen as cg
import esphome.config_validation as cv

CODEOWNERS = ["@your_username"]
DEPENDENCIES = ['web_server', 'sd_mmc_card']
MULTI_CONF = True

webdavbox3_ns = cg.esphome_ns.namespace('webdavbox3')
WebDAVBox3 = webdavbox3_ns.class_('WebDAVBox3')

CONFIG_SCHEMA = cv.Schema({
    cv.Optional('root_path', default='/sdcard/'): cv.string,
})

async def to_code(config):
    # Instanciation correcte de la classe WebDAVBox3
    var = cg.new_Pvariable(WebDAVBox3)
    
    if 'root_path' in config:
        cg.add(var.set_root_path(config['root_path']))
    
    return var


