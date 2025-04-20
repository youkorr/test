#include "webdavbox3.h"
#include "esphome/core/log.h"
#include "../sd_mmc_card/sd_mmc_card.h"
#include <esp_http_server.h>
#include <cstring>

namespace esphome {
namespace webdavbox3 {

static const char *TAG = "webdavbox3";

void WebDAVBox3::setup() {
  if (!sd_mmc_card::SDMMCCard::is_mounted()) {
    ESP_LOGE(TAG, "SD card not mounted, WebDAV server will not start");
    return;
  }

  this->start_server();
}

void WebDAVBox3::start_server() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.uri_match_fn = httpd_uri_match_wildcard;

  if (httpd_start(&this->server_, &config) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start WebDAV server");
    return;
  }

  httpd_uri_t get_uri = {
    .uri = "/*",
    .method = HTTP_GET,
    .handler = handle_get,
    .user_ctx = this
  };
  httpd_register_uri_handler(this->server_, &get_uri);

  httpd_uri_t put_uri = {
    .uri = "/*",
    .method = HTTP_PUT,
    .handler = handle_put,
    .user_ctx = this
  };
  httpd_register_uri_handler(this->server_, &put_uri);

  httpd_uri_t delete_uri = {
    .uri = "/*",
    .method = HTTP_DELETE,
    .handler = handle_delete,
    .user_ctx = this
  };
  httpd_register_uri_handler(this->server_, &delete_uri);

  httpd_uri_t propfind_uri = {
    .uri = "/*",
    .method = HTTP_PROPFIND,
    .handler = handle_propfind,
    .user_ctx = this
  };
  httpd_register_uri_handler(this->server_, &propfind_uri);

  ESP_LOGI(TAG, "WebDAV server started");
}

void WebDAVBox3::dump_config() {
  ESP_LOGCONFIG(TAG, "WebDAVBox3:");
}

std::string WebDAVBox3::get_file_path(httpd_req_t *req, const std::string &root_path) {
  std::string uri = req->uri;
  std::string path = root_path + uri;
  return path;
}

esp_err_t WebDAVBox3::handle_get(httpd_req_t *req) {
  auto *webdav = static_cast<WebDAVBox3 *>(req->user_ctx);
  std::string file_path = get_file_path(req, webdav->root_path_);

  FILE *file = fopen(file_path.c_str(), "rb");
  if (!file) {
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
    return ESP_FAIL;
  }

  char buffer[512];
  size_t read_bytes;
  while ((read_bytes = fread(buffer, 1, sizeof(buffer), file)) > 0) {
    httpd_resp_send_chunk(req, buffer, read_bytes);
  }
  fclose(file);
  httpd_resp_send_chunk(req, nullptr, 0);
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_put(httpd_req_t *req) {
  auto *webdav = static_cast<WebDAVBox3 *>(req->user_ctx);
  std::string file_path = get_file_path(req, webdav->root_path_);

  FILE *file = fopen(file_path.c_str(), "wb");
  if (!file) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open file for writing");
    return ESP_FAIL;
  }

  char buffer[512];
  int received;
  while ((received = httpd_req_recv(req, buffer, sizeof(buffer))) > 0) {
    fwrite(buffer, 1, received, file);
  }
  fclose(file);
  httpd_resp_sendstr(req, "File uploaded successfully");
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_delete(httpd_req_t *req) {
  auto *webdav = static_cast<WebDAVBox3 *>(req->user_ctx);
  std::string file_path = get_file_path(req, webdav->root_path_);

  if (remove(file_path.c_str()) != 0) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to delete file");
    return ESP_FAIL;
  }

  httpd_resp_sendstr(req, "File deleted");
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_propfind(httpd_req_t *req) {
  auto *webdav = static_cast<WebDAVBox3 *>(req->user_ctx);
  std::string file_path = get_file_path(req, webdav->root_path_);

  struct stat st;
  if (stat(file_path.c_str(), &st) != 0) {
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File or directory not found");
    return ESP_FAIL;
  }

  char response[256];
  snprintf(response, sizeof(response), "<file><path>%s</path><size>%ld</size></file>",
           file_path.c_str(), st.st_size);

  httpd_resp_set_type(req, "text/xml");
  httpd_resp_send(req, response, strlen(response));
  return ESP_OK;
}

}  // namespace webdavbox3
}  // namespace esphome



