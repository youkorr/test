#include "samba_server.h"
#include "esphome/core/log.h"

namespace esphome {
namespace samba_server {

static const char *TAG = "samba_server";

void SambaServer::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Samba Server...");
  
  web_server_ = std::make_unique<WebServer>();
  web_server_->setup();
  
  // TODO: Implement SMB protocol
}

void SambaServer::loop() {
  web_server_->loop();
  
  // TODO: Handle SMB connections
}

}  // namespace samba_server
}  // namespace esphome



