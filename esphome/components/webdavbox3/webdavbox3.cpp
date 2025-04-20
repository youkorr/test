#include "webdavbox3.h"
#include "../sd_mmc_card/sd_mmc_card.h"
#include "esphome/core/log.h"
#include <esp_log.h>
#include <esp_err.h>
#include <esp_http_server.h>
#include <dirent.h>
#include <sys/stat.h>

namespace esphome {
namespace webdavbox3 {

static const char *const TAG = "webdavbox3";

void WebDAVBox3::setup() {
  if (!sd_mmc_card::is_mounted()) {
    ESP_LOGE(TAG, "SD card not mounted. Aborting WebDAV setup.");
    return;
  }

  this->start_server();
}

void WebDAVBox3::start_server() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = this->port_;
  config.stack_size = 8192;
  config.ctrl_port = this->port_ + 1000;

  esp_err_t err = httpd_start(&this->server_, &config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start server on port %d: %s", config.server_port, esp_err_to_name(err));
    return;
  }

  ESP_LOGI(TAG, "WebDAV server started on port %d", config.server_port);

  httpd_uri_t get_uri = {
    .uri = this->url_prefix_.c_str(),
    .method = HTTP_GET,
    .handler = handle_get,
    .user_ctx = this
  };
  httpd_register_uri_handler(this->server_, &get_uri);

  httpd_uri_t put_uri = {
    .uri = this->url_prefix_.c_str(),
    .method = HTTP_PUT,
    .handler = handle_put,
    .user_ctx = this
  };
  httpd_register_uri_handler(this->server_, &put_uri);

  httpd_uri_t delete_uri = {
    .uri = this->url_prefix_.c_str(),
    .method = HTTP_DELETE,
    .handler = handle_delete,
    .user_ctx = this
  };
  httpd_register_uri_handler(this->server_, &delete_uri);

  httpd_uri_t propfind_uri = {
    .uri = this->url_prefix_.c_str(),
    .method = HTTP_PROPFIND,
    .handler = handle_propfind,
    .user_ctx = this
  };
  httpd_register_uri_handler(this->server_, &propfind_uri);
}

// ---------------- GET handler ----------------
esp_err_t WebDAVBox3::handle_get(httpd_req_t *req) {
  auto *webdav = (WebDAVBox3 *)req->user_ctx;
  std::string filepath = webdav->get_file_path(req->uri);

  FILE *file = fopen(filepath.c_str(), "rb");
  if (!file) {
    return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
  }

  char buffer[1024];
  size_t read_bytes;
  while ((read_bytes = fread(buffer, 1, sizeof(buffer), file)) > 0) {
    httpd_resp_send_chunk(req, buffer, read_bytes);
  }
  fclose(file);
  return httpd_resp_send_chunk(req, nullptr, 0);
}

// ---------------- PUT handler ----------------
esp_err_t WebDAVBox3::handle_put(httpd_req_t *req) {
  auto *webdav = (WebDAVBox3 *)req->user_ctx;
  std::string filepath = webdav->get_file_path(req->uri);

  FILE *file = fopen(filepath.c_str(), "wb");
  if (!file) {
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open file");
  }

  char buffer[1024];
  int received;
  while ((received = httpd_req_recv(req, buffer, sizeof(buffer))) > 0) {
    fwrite(buffer, 1, received, file);
  }
  fclose(file);
  return httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
}

// ---------------- DELETE handler ----------------
esp_err_t WebDAVBox3::handle_delete(httpd_req_t *req) {
  auto *webdav = (WebDAVBox3 *)req->user_ctx;
  std::string filepath = webdav->get_file_path(req->uri);

  if (remove(filepath.c_str()) == 0) {
    return httpd_resp_send(req, "Deleted", HTTPD_RESP_USE_STRLEN);
  } else {
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Delete failed");
  }
}

// ---------------- PROPFIND handler ----------------
esp_err_t WebDAVBox3::handle_propfind(httpd_req_t *req) {
  auto *webdav = (WebDAVBox3 *)req->user_ctx;
  std::string folder = webdav->get_file_path(req->uri);

  DIR *dir = opendir(folder.c_str());
  if (!dir) {
    return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Directory not found");
  }

  std::string response = "<?xml version=\"1.0\"?><d:multistatus xmlns:d=\"DAV:\">";

  struct dirent *entry;
  while ((entry = readdir(dir)) != nullptr) {
    std::string name = entry->d_name;
    response += "<d:response><d:href>" + name + "</d:href></d:response>";
  }
  closedir(dir);
  response += "</d:multistatus>";

  httpd_resp_set_type(req, "application/xml");
  return httpd_resp_send(req, response.c_str(), response.size());
}

// ---------------- Utils ----------------
std::string WebDAVBox3::get_file_path(const char *uri) const {
  std::string path = uri;
  if (path.find(this->url_prefix_) == 0) {
    path = path.substr(this->url_prefix_.size());
  }
  return this->root_path_ + path;
}

}  // namespace webdavbox3
}  // namespace esphome


