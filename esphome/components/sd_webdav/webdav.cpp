#include "webdav.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include <SD.h>

namespace esphome {
namespace sd_webdav {

void SDWebDAVComponent::setup() {
  web_server_ = new web_server_base::WebServerBase();
  web_server_->init();
  
  web_server_->on("/{*}", HTTP_GET, [this](AsyncWebServerRequest *request) {
    if (!this->authenticate()) {
      return request->requestAuthentication();
    }
    this->handle_get(request);
  });

  web_server_->on("/{*}", HTTP_PUT, [this](AsyncWebServerRequest *request) {
    if (!this->authenticate()) {
      return request->requestAuthentication();
    }
    this->handle_put(request);
  });

  web_server_->on("/{*}", HTTP_PROPFIND, [this](AsyncWebServerRequest *request) {
    if (!this->authenticate()) {
      return request->requestAuthentication();
    }
    this->handle_propfind(request);
  });

  web_server_->on("/{*}", HTTP_DELETE, [this](AsyncWebServerRequest *request) {
    if (!this->authenticate()) {
      return request->requestAuthentication();
    }
    this->handle_delete(request);
  });

  web_server_->on("/{*}", HTTP_MKCOL, [this](AsyncWebServerRequest *request) {
    if (!this->authenticate()) {
      return request->requestAuthentication();
    }
    this->handle_mkcol(request);
  });
}

void SDWebDAVComponent::handle_get(AsyncWebServerRequest *request) {
  String path = mount_point_ + request->url();
  if (SD.exists(path)) {
    if (SD.isDirectory(path)) {
      request->send(405, "text/plain", "Cannot GET a directory");
    } else {
      request->send(SD, path);
    }
  } else {
    request->send(404, "text/plain", "File not found");
  }
}

void SDWebDAVComponent::handle_put(AsyncWebServerRequest *request) {
  String path = mount_point_ + request->url();
  if (request->hasParam("file", true)) {
    File file = SD.open(path, FILE_WRITE);
    if (file) {
      file.print(request->getParam("file", true)->value());
      file.close();
      request->send(201);
    } else {
      request->send(500, "text/plain", "Failed to create file");
    }
  } else {
    request->send(400, "text/plain", "No file data provided");
  }
}

void SDWebDAVComponent::handle_propfind(AsyncWebServerRequest *request) {
  String path = mount_point_ + request->url();
  String response = "<?xml version=\"1.0\" encoding=\"utf-8\"?>";
  response += "<d:multistatus xmlns:d=\"DAV:\">";
  
  if (SD.exists(path)) {
    File file = SD.open(path);
    response += "<d:response>";
    response += "<d:href>" + request->url() + "</d:href>";
    response += "<d:propstat>";
    response += "<d:prop>";
    response += "<d:getlastmodified>" + String(file.getLastWrite()) + "</d:getlastmodified>";
    response += "<d:getcontentlength>" + String(file.size()) + "</d:getcontentlength>";
    response += "<d:resourcetype>" + (file.isDirectory() ? "<d:collection/>" : "") + "</d:resourcetype>";
    response += "</d:prop>";
    response += "<d:status>HTTP/1.1 200 OK</d:status>";
    response += "</d:propstat>";
    response += "</d:response>";
    file.close();
  }
  
  response += "</d:multistatus>";
  request->send(207, "application/xml; charset=utf-8", response);
}

void SDWebDAVComponent::handle_delete(AsyncWebServerRequest *request) {
  String path = mount_point_ + request->url();
  if (SD.exists(path)) {
    if (SD.remove(path)) {
      request->send(204);
    } else {
      request->send(500, "text/plain", "Failed to delete file");
    }
  } else {
    request->send(404, "text/plain", "File not found");
  }
}

void SDWebDAVComponent::handle_mkcol(AsyncWebServerRequest *request) {
  String path = mount_point_ + request->url();
  if (SD.mkdir(path)) {
    request->send(201);
  } else {
    request->send(500, "text/plain", "Failed to create directory");
  }
}

void SDWebDAVComponent::loop() {
  // Handle WebDAV requests
}

void SDWebDAVComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "SD WebDAV:");
  ESP_LOGCONFIG(TAG, "  Mount Point: %s", mount_point_.c_str());
  ESP_LOGCONFIG(TAG, "  Username: %s", username_.c_str());
}

bool SDWebDAVComponent::authenticate() {
  if (username_.empty() && password_.empty()) {
    return true;
  }
  return web_server_->check_auth(username_, password_);
}

}  // namespace sd_webdav
}  // namespace esphome
