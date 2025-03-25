import esphome.codegen as cg
import esphome.config_validation as cv

DEPENDENCIES = ['web_server', 'sd_card']
MULTI_CONF = True

webdavbox3_ns = cg.esphome_ns.namespace('webdavbox3')
WebDAVBox3 = webdavbox3_ns.class_('WebDAVBox3')

CONFIG_SCHEMA = cv.Schema({
    cv.Optional('root_path', default='/'): cv.string,
})

async def to_code(config):
    # Créer une instance de WebDAVBox3
    var = cg.new_Pvariable(config)
    
    # Configurer le chemin racine
    cg.add(var.set_root_path(config['root_path']))
    
    return var
