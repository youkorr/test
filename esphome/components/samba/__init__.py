import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ['esp32']
AUTO_LOAD = ['async_tcp']

samba_ns = cg.esphome_ns.namespace('samba_server')
SambaServer = samba_ns.class_('SambaServer', cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(SambaServer),
    cv.Optional('workgroup', default='WORKGROUP'): cv.string,
    cv.Optional('share_name', default='ESP32'): cv.string,
    cv.Optional('username', default='esp32'): cv.string,
    cv.Optional('password', default='password'): cv.string,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    cg.add(var.set_workgroup(config['workgroup']))
    cg.add(var.set_share_name(config['share_name']))
    cg.add(var.set_username(config['username']))
    cg.add(var.set_password(config['password']))


