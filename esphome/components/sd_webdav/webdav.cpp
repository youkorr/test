#include "webdav.h"

namespace esphome {
namespace sd_webdav {

void SDWebDAV::setup() {
  server_ = new AsyncWebServer(80);
  dav_ = new AsyncWebdav(mount_point_.c_str());
  
  if (!username_.empty() && !password_.empty()) {
    dav_->setAuthentication(username_.c_str(), password_.c_str());
  }

  server_->addHandler(dav_);
  server_->begin();
}

void SDWebDAV::loop() {
  // Gestion des requêtes
}

}  // namespace sd_webdav
}  // namespace esphome

