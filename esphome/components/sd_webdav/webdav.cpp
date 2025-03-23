#include "webdav.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/components/sdcard/sdcard.h"

namespace esphome {
namespace sd_webdav {

void SDWebDAVComponent::setup() {
  web_server_ = new web_server_base::WebServerBase();
  
  // Initialize WebDAV server
  web_server_->init();
  
  // Set up authentication if credentials provided
  if (!username_.empty() && !password_.empty()) {
    web_server_->set_authentication(username_.c_str(), password_.c_str());
  }
  
  // Mount SD card
  if (!sd_card_->mount(mount_point_.c_str())) {
    ESP_LOGE(TAG, "Failed to mount SD card!");
    return;
  }
  
  // Set up WebDAV routes
  web_server_->on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "WebDAV Server Running");
  });
  
  // Start server
  web_server_->start();
}

void SDWebDAVComponent::loop() {
  // Handle WebDAV requests
}

void SDWebDAVComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "WebDAV Server:");
  ESP_LOGCONFIG(TAG, "  Mount Point: %s", mount_point_.c_str());
  ESP_LOGCONFIG(TAG, "  Username: %s", username_.c_str());
  ESP_LOGCONFIG(TAG, "  Password: %s", password_.empty() ? "not set" : "set");
}

}  // namespace sd_webdav
}  // namespace esphome
