import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import web_server

DEPENDENCIES = ['web_server', 'sd_card']
MULTI_CONF = True

webdavbox3_ns = cg.esphome_ns.namespace('webdavbox3')
WebDAVBox3 = webdavbox3_ns.class_('WebDAVBox3', web_server.WebServer)

CONFIG_SCHEMA = web_server.WEB_SERVER_SCHEMA.extend({
    cv.Optional('root_path', default='/'): cv.string,
})

async def to_code(config):
    # Récupérer le composant web_server existant
    web_server_component = await web_server.to_code(config)
    
    # Créer une instance de WebDAVBox3
    cg.add(web_server_component.add_extension(WebDAVBox3))
    
    # Configurer le chemin racine
    cg.add(web_server_component.set_root_path(config['root_path']))
