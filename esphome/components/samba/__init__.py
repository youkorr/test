import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.components import web_server_base

DEPENDENCIES = ['web_server_base']
AUTO_LOAD = ['web_server_base']

samba_ns = cg.esphome_ns.namespace('samba')
SambaComponent = samba_ns.class_('SambaComponent', cg.Component)

CONF_WORKGROUP = 'workgroup'
CONF_ROOT_PATH = 'root_path'
CONF_SD_MMC_CARD = 'sd_mmc_card'
CONF_LOCAL_MASTER = 'local_master'
CONF_USERNAME = 'username'
CONF_PASSWORD = 'password'
CONF_ENABLED_SHARES = 'enabled_shares'
CONF_ALLOW_HOSTS = 'allow_hosts'

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(SambaComponent),
    cv.Required(CONF_WORKGROUP): cv.string,
    cv.Required(CONF_ROOT_PATH): cv.string,
    cv.Required(CONF_SD_MMC_CARD): cv.use_id(cg.Component),
    cv.Optional(CONF_LOCAL_MASTER, default=True): cv.boolean,
    cv.Required(CONF_USERNAME): cv.string,
    cv.Required(CONF_PASSWORD): cv.string,
    cv.Optional(CONF_ENABLED_SHARES, default=[]): cv.ensure_list(cv.string),
    cv.Optional(CONF_ALLOW_HOSTS, default=[]): cv.ensure_list(cv.string),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_workgroup(config[CONF_WORKGROUP]))
    cg.add(var.set_root_path(config[CONF_ROOT_PATH]))
    sd_mmc = await cg.get_variable(config[CONF_SD_MMC_CARD])
    cg.add(var.set_sd_mmc_card(sd_mmc))
    cg.add(var.set_local_master(config[CONF_LOCAL_MASTER]))
    cg.add(var.set_username(config[CONF_USERNAME]))
    cg.add(var.set_password(config[CONF_PASSWORD]))

    for share in config[CONF_ENABLED_SHARES]:
        cg.add(var.add_enabled_share(share))

    for host in config[CONF_ALLOW_HOSTS]:
        cg.add(var.add_allowed_host(host))







