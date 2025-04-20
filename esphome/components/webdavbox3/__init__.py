import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PORT

# Définir des constantes pour les options username et password
CONF_USERNAME = "username"
CONF_PASSWORD = "password"

CODEOWNERS = ["@your_username"]
DEPENDENCIES = ['web_server', 'sd_mmc']
MULTI_CONF = True

webdavbox3_ns = cg.esphome_ns.namespace('webdavbox3')
WebDAVBox3 = webdavbox3_ns.class_('WebDAVBox3', cg.Component)

# Fonction pour valider les options username et password
def validate_auth(config):
    if (CONF_USERNAME in config) != (CONF_PASSWORD in config):
        raise cv.Invalid("Both username and password must be specified together")
    return config

# Schéma de configuration, incluant username et password
CONFIG_SCHEMA = cv.All(
    cv.Schema({
        cv.Required(CONF_ID): cv.declare_id(WebDAVBox3),
        cv.Optional('root_path', default='/sdcard/'): cv.string,
        cv.Optional('url_prefix', default='/webdav'): cv.string,
        cv.Optional(CONF_PORT, default=8080): cv.port,
        # Assurer que username et password sont bien ajoutés ici
        cv.Optional(CONF_USERNAME, default=""): cv.string,
        cv.Optional(CONF_PASSWORD, default=""): cv.string,
    }).extend(cv.COMPONENT_SCHEMA),  # Extends pour inclure les composants standards
    validate_auth  # Validation des options
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Configurer les chemins et autres options
    cg.add(var.set_root_path(config['root_path']))
    cg.add(var.set_url_prefix(config['url_prefix']))
    cg.add(var.set_port(config[CONF_PORT]))

    # Ajouter username et password si présents
    if CONF_USERNAME in config:
        cg.add(var.set_username(config[CONF_USERNAME]))
    if CONF_PASSWORD in config:
        cg.add(var.set_password(config[CONF_PASSWORD]))

    return var







