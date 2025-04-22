#include "webdavbox3.h"
#include "esphome/core/log.h"
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <fstream>

namespace esphome {
namespace webdavbox3 {

static const char *const TAG = "webdavbox3";

void WebDAVBox3::setup() {
  ESP_LOGI(TAG, "Utilisation du montage SD existant à %s", root_path_.c_str());
  this->configure_http_server();
  this->start_server();
}

void WebDAVBox3::loop() {
  // Rien pour le moment
}

void WebDAVBox3::configure_http_server() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = port_;
  config.ctrl_port = port_ + 1000;  // évite conflit avec l'autre HTTPD si existant

  if (httpd_start(&server_, &config) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start server on port %d", port_);
    server_ = nullptr;
    return;
  }

  ESP_LOGI(TAG, "Serveur WebDAV démarré sur le port %d", port_);

  httpd_uri_t root_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = handle_root,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &root_uri);

  httpd_uri_t propfind_uri = {
    .uri = "/*",
    .method = HTTP_PROPFIND,
    .handler = handle_webdav_propfind,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &propfind_uri);

  httpd_uri_t get_uri = {
    .uri = "/*",
    .method = HTTP_GET,
    .handler = handle_webdav_get,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &get_uri);

  httpd_uri_t put_uri = {
    .uri = "/*",
    .method = HTTP_PUT,
    .handler = handle_webdav_put,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &put_uri);

  httpd_uri_t delete_uri = {
    .uri = "/*",
    .method = HTTP_DELETE,
    .handler = handle_webdav_delete,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &delete_uri);

  httpd_uri_t mkcol_uri = {
    .uri = "/*",
    .method = HTTP_MKCOL,
    .handler = handle_webdav_mkcol,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &mkcol_uri);

  httpd_uri_t move_uri = {
    .uri = "/*",
    .method = HTTP_MOVE,
    .handler = handle_webdav_move,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &move_uri);

  httpd_uri_t copy_uri = {
    .uri = "/*",
    .method = HTTP_COPY,
    .handler = handle_webdav_copy,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &copy_uri);
}

void WebDAVBox3::start_server() {
  if (server_ != nullptr)
    return;
  configure_http_server();
}

void WebDAVBox3::stop_server() {
  if (server_ != nullptr) {
    httpd_stop(server_);
    server_ = nullptr;
  }
}

std::string WebDAVBox3::get_file_path(httpd_req_t *req, const std::string &root_path) {
  std::string uri = req->uri;
  std::string path = root_path + uri;
  return path;
}

bool WebDAVBox3::is_dir(const std::string &path) {
  struct stat st;
  if (stat(path.c_str(), &st) == 0)
    return S_ISDIR(st.st_mode);
  return false;
}

std::vector<std::string> WebDAVBox3::list_dir(const std::string &path) {
  std::vector<std::string> files;
  DIR *dir = opendir(path.c_str());
  if (dir != nullptr) {
    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
      if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
        files.push_back(entry->d_name);
      }
    }
    closedir(dir);
  }
  return files;
}

// ========== HANDLERS ==========

esp_err_t WebDAVBox3::handle_root(httpd_req_t *req) {
  httpd_resp_send(req, "ESP32 WebDAV Server OK", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_propfind(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  std::string path = get_file_path(req, inst->root_path_);

  ESP_LOGD(TAG, "PROPFIND sur %s", path.c_str());

  if (is_dir(path)) {
    std::string response = "<?xml version=\"1.0\"?>\n"
                           "<D:multistatus xmlns:D=\"DAV:\">\n";
    for (const auto &file : list_dir(path)) {
      response += "  <D:response><D:href>/" + file + "</D:href></D:response>\n";
    }
    response += "</D:multistatus>";
    httpd_resp_set_type(req, "application/xml");
    httpd_resp_send(req, response.c_str(), response.length());
    return ESP_OK;
  }

  return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not Found");
}

esp_err_t WebDAVBox3::handle_webdav_get(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  std::string path = get_file_path(req, inst->root_path_);

  ESP_LOGD(TAG, "GET %s", path.c_str());

  FILE *file = fopen(path.c_str(), "rb");
  if (!file)
    return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");

  char buffer[1024];
  size_t read_bytes;
  httpd_resp_set_type(req, "application/octet-stream");

  while ((read_bytes = fread(buffer, 1, sizeof(buffer), file)) > 0) {
    httpd_resp_send_chunk(req, buffer, read_bytes);
  }

  fclose(file);
  httpd_resp_send_chunk(req, nullptr, 0);
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_put(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  std::string path = get_file_path(req, inst->root_path_);

  ESP_LOGD(TAG, "PUT %s", path.c_str());

  FILE *file = fopen(path.c_str(), "wb");
  if (!file)
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");

  char buffer[1024];
  int received;

  while ((received = httpd_req_recv(req, buffer, sizeof(buffer))) > 0) {
    fwrite(buffer, 1, received, file);
  }

  fclose(file);
  httpd_resp_send(req, "File uploaded", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_delete(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  std::string path = get_file_path(req, inst->root_path_);

  ESP_LOGD(TAG, "DELETE %s", path.c_str());

  if (remove(path.c_str()) == 0) {
    httpd_resp_send(req, "Deleted", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }

  return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
}

esp_err_t WebDAVBox3::handle_webdav_mkcol(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  std::string path = get_file_path(req, inst->root_path_);

  ESP_LOGD(TAG, "MKCOL %s", path.c_str());

  if (mkdir(path.c_str(), 0755) == 0) {
    httpd_resp_send(req, "Directory created", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }

  return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create directory");
}

esp_err_t WebDAVBox3::handle_webdav_move(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  std::string src = get_file_path(req, inst->root_path_);

  char dest_uri[512];
  if (httpd_req_get_hdr_value_str(req, "Destination", dest_uri, sizeof(dest_uri)) == ESP_OK) {
    std::string dst = inst->root_path_ + std::string(dest_uri);
    if (rename(src.c_str(), dst.c_str()) == 0) {
      httpd_resp_send(req, "Moved", HTTPD_RESP_USE_STRLEN);
      return ESP_OK;
    }
  }

  return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Move failed");
}

esp_err_t WebDAVBox3::handle_webdav_copy(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  std::string src = get_file_path(req, inst->root_path_);

  char dest_uri[512];
  if (httpd_req_get_hdr_value_str(req, "Destination", dest_uri, sizeof(dest_uri)) == ESP_OK) {
    std::string dst = inst->root_path_ + std::string(dest_uri);
    std::ifstream in(src, std::ios::binary);
    std::ofstream out(dst, std::ios::binary);

    if (!in || !out)
      return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Copy failed");

    out << in.rdbuf();
    httpd_resp_send(req, "Copied", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }

  return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Copy failed");
}

}  // namespace webdavbox3
}  // namespace esphome


















