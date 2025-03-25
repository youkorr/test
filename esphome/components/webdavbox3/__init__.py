import esphome.codegen as cg
import esphome.config_validation as cv

CODEOWNERS = ["@youkorr"]
DEPENDENCIES = ['web_server', 'sd_mmc_card']
MULTI_CONF = True

webdavbox3_ns = cg.esphome_ns.namespace('webdavbox3')
WebDAVBox3 = webdavbox3_ns.class_('WebDAVBox3', cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.Optional('root_path', default='/sdcard/'): cv.string,
}).extend(cv.COMPONENT_SCHEMA)  # Important: utilise le schéma de base du composant

async def to_code(config):
    # Crée un ID automatique si non spécifié
    var = cg.new_Pvariable(config[CONF_ID])  # Utilise l'ID généré automatiquement
    
    await cg.register_component(var, config)
    
    if 'root_path' in config:
        cg.add(var.set_root_path(config['root_path']))
    
    return var

