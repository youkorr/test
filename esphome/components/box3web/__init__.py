import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE

DEPENDENCIES = ['esp32']
CODEOWNERS = ['@your-github-username']

box3web_ns = cg.esphome_ns.namespace('box3web')
Box3WebComponent = box3web_ns.class_('Box3WebComponent', cg.Component)

CONF_BOX3WEB = 'box3web'
CONF_URL_PREFIX = 'url_prefix'
CONF_ROOT_PATH = 'root_path'
CONF_ENABLE_DELETION = 'enable_deletion'
CONF_ENABLE_DOWNLOAD = 'enable_download'
CONF_ENABLE_UPLOAD = 'enable_upload'

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(Box3WebComponent),
    cv.Optional(CONF_URL_PREFIX, default='files'): cv.string,
    cv.Optional(CONF_ROOT_PATH, default='/'): cv.string,
    cv.Optional(CONF_ENABLE_DELETION, default=False): cv.boolean,
    cv.Optional(CONF_ENABLE_DOWNLOAD, default=True): cv.boolean,
    cv.Optional(CONF_ENABLE_UPLOAD, default=False): cv.boolean,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    cg.add(var.set_url_prefix(config[CONF_URL_PREFIX]))
    cg.add(var.set_root_path(config[CONF_ROOT_PATH]))
    cg.add(var.set_enable_deletion(config[CONF_ENABLE_DELETION]))
    cg.add(var.set_enable_download(config[CONF_ENABLE_DOWNLOAD]))
    cg.add(var.set_enable_upload(config[CONF_ENABLE_UPLOAD]))
    
    cg.add_build_flag('-DUSE_BOX3WEB')
    
    if CORE.is_esp32:
        cg.add_platformio_option("board_build.partitions", "min_spiffs.csv")
