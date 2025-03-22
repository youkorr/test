#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "../sd_mmc_card/sd_mmc_card.h"
#include <esp_http_server.h>

namespace esphome {
namespace box3web {

class Box3WebComponent : public Component {
 public:
  Box3WebComponent();
  
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  
  void set_url_prefix(const std::string &url_prefix) { this->url_prefix_ = url_prefix; }
  void set_root_path(const std::string &root_path) { this->root_path_ = root_path; }
  void set_enable_deletion(bool enable) { this->enable_deletion_ = enable; }
  void set_enable_download(bool enable) { this->enable_download_ = enable; }
  void set_enable_upload(bool enable) { this->enable_upload_ = enable; }
  
 protected:
  std::string url_prefix_{"files"};
  std::string root_path_{"/"};
  bool enable_deletion_{false};
  bool enable_download_{true};
  bool enable_upload_{false};
  
  httpd_handle_t server_{nullptr};
  
  // Helper methods
  static esp_err_t index_handler(httpd_req_t *req);
  static esp_err_t file_handler(httpd_req_t *req);
  static esp_err_t upload_handler(httpd_req_t *req);
  static esp_err_t delete_handler(httpd_req_t *req);
  static esp_err_t download_handler(httpd_req_t *req);
  
  // Static instance pointer for use in C callbacks
  static Box3WebComponent *instance;
  
  // Private methods
  void setup_server();
  void register_handlers();
  bool is_valid_path(const std::string &path);
  std::string get_content_type(const std::string &path);
};

} // namespace box3web
} // namespace esphome
