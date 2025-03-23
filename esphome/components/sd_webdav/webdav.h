#pragma once
#include "webdav.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "../sd_mmc_card/sd_mmc_card.h"

namespace esphome {
namespace sd_webdav {

static const char *const TAG = "webdav";

void SDWebDAVComponent::setup() {
  web_server_ = new web_server_base::WebServerBase();
  
  // Initialize WebDAV server
  web_server_->init();
  
  // Initialize SD card
  sd_card_->setup();
  if (!sd_card_->is_ready()) {
    ESP_LOGE(TAG, "Failed to initialize SD card!");
    return;
  }
  
  // Set up WebDAV routes
  web_server_->add_handler("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "WebDAV Server Running");
  });
  
  // Start server
  web_server_->begin();
}

void SDWebDAVComponent::loop() {
  // Handle WebDAV requests
}

void SDWebDAVComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "WebDAV Server:");
  ESP_LOGCONFIG(TAG, "  Mount Point: %s", mount_point_.c_str());
}

}  // namespace sd_webdav
}  // namespace esphome







