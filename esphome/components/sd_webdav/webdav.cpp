#include "webdav.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace webdav {

static const char *TAG = "webdav";

void WebDAVComponent::setup() {
  if (this->sd_card_ == nullptr) {
    ESP_LOGE(TAG, "SD card not configured");
    return;
  }
  if (!this->sd_card_->is_mounted()) {
    ESP_LOGE(TAG, "SD card not mounted");
    return;
  }
  ESP_LOGI(TAG, "WebDAV server started on %s", this->mount_point_.c_str());
}

bool WebDAVComponent::canHandle(AsyncWebServerRequest *request) {
  return request->url().startsWith(this->mount_point_);
}

void WebDAVComponent::handleRequest(AsyncWebServerRequest *request) {
  if (!this->authenticate(request)) {
    request->requestAuthentication();
    return;
  }

  if (request->method() == HTTP_GET) {
    this->handleGet(request);
  } else if (request->method() == HTTP_PUT) {
    this->handlePut(request);
  } else if (request->method() == HTTP_DELETE) {
    this->handleDelete(request);
  } else if (request->method() == HTTP_MKCOL) {
    this->handleMkcol(request);
  } else if (request->method() == HTTP_PROPFIND) {
    this->handlePropfind(request);
  } else {
    request->send(405);
  }
}

bool WebDAVComponent::authenticate(AsyncWebServerRequest *request) {
  if (this->username_.empty() && this->password_.empty()) {
    return true;
  }
  return request->authenticate(this->username_.c_str(), this->password_.c_str());
}

void WebDAVComponent::handlePropfind(AsyncWebServerRequest *request) {
  request->send(207, "application/xml", "<d:multistatus xmlns:d=\"DAV:\"></d:multistatus>");
}

void WebDAVComponent::handleGet(AsyncWebServerRequest *request) {
  String path = request->url().substring(this->mount_point_.length());
  if (this->sd_card_->fs()->exists(path)) {
    File file = this->sd_card_->fs()->open(path);
    if (file) {
      request->send(*this->sd_card_->fs(), path, "application/octet-stream");
      file.close();
    } else {
      request->send(404);
    }
  } else {
    request->send(404);
  }
}

void WebDAVComponent::handlePut(AsyncWebServerRequest *request) {
  String path = request->url().substring(this->mount_point_.length());
  if (request->hasParam("body", true)) {
    AsyncWebParameter* p = request->getParam("body", true);
    File file = this->sd_card_->fs()->open(path, FILE_WRITE);
    if (file) {
      file.print(p->value());
      file.close();
      request->send(201);
    } else {
      request->send(500);
    }
  } else {
    request->send(400);
  }
}

void WebDAVComponent::handleDelete(AsyncWebServerRequest *request) {
  String path = request->url().substring(this->mount_point_.length());
  if (this->sd_card_->fs()->remove(path)) {
    request->send(204);
  } else {
    request->send(404);
  }
}

void WebDAVComponent::handleMkcol(AsyncWebServerRequest *request) {
  String path = request->url().substring(this->mount_point_.length());
  if (this->sd_card_->fs()->mkdir(path)) {
    request->send(201);
  } else {
    request->send(500);
  }
}

}  // namespace webdav
}  // namespace esphome


