#pragma once

#include "esp_http_server.h"
#include "esp_log.h"
#include <string>

namespace esphome {
namespace samba_server {

class WebServer {
 public:
  void setup();
  void loop() {} // ESP-IDF HTTP server doesn't need a loop

 private:
  httpd_handle_t server_ = NULL;
  
  static esp_err_t handle_list_directory(httpd_req_t *req);
  static esp_err_t handle_download_file(httpd_req_t *req);
  static esp_err_t handle_upload_file(httpd_req_t *req);
  static esp_err_t handle_delete_file(httpd_req_t *req);
  static esp_err_t handle_rename_file(httpd_req_t *req);
};

}  // namespace samba_server
}  // namespace esphome


