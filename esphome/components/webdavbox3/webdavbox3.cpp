#include "webdavbox3.h"
#include "esphome/core/log.h"
#include <sys/stat.h>
#include <dirent.h>
#include <algorithm>
#include <sstream>
#include <ctime>
#include <cstring>
#include <errno.h>

namespace esphome {
namespace webdavbox3 {

static const char *TAG = "webdavbox3";

void WebDAVBox3::setup() {
  ESP_LOGI(TAG, "Setting up WebDAV server on port %d", this->port_);
  ESP_LOGI(TAG, "Root path: %s", this->root_path_.c_str());
  
  // S'assurer que le dossier racine existe
  if (mkdir(this->root_path_.c_str(), 0755) != 0 && errno != EEXIST) {
    ESP_LOGE(TAG, "Failed to create root directory: %s (errno: %d)", this->root_path_.c_str(), errno);
  }
  
  // Configurer et démarrer le serveur HTTP
  this->configure_http_server();
}

void WebDAVBox3::loop() {
  // Nothing to do in loop
}

void WebDAVBox3::configure_http_server() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  
  // Configuration du serveur
  config.server_port = this->port_;
  config.max_uri_handlers = 15;
  config.stack_size = 8192;
  config.max_resp_headers = 32;
  config.max_open_sockets = 7;
  config.lru_purge_enable = true;
  config.uri_match_fn = httpd_uri_match_wildcard;
  config.core_id = 0;
  config.task_priority = 5;
  
  ESP_LOGI(TAG, "Starting HTTP server on port: %d", config.server_port);
  
  esp_err_t err = httpd_start(&this->server_, &config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start HTTP server (err: %d)", err);
    return;
  }
  
  ESP_LOGI(TAG, "HTTP server started successfully");

  // Configuration des gestionnaires d'URI
  const httpd_uri_t uri_handlers[] = {
    {
      .uri = "/*",
      .method = HTTP_GET,
      .handler = handle_webdav_get,
      .user_ctx = this
    },
    {
      .uri = "/*",
      .method = HTTP_PUT,
      .handler = handle_webdav_put,
      .user_ctx = this
    },
    {
      .uri = "/*",
      .method = HTTP_DELETE,
      .handler = handle_webdav_delete,
      .user_ctx = this
    },
    {
      .uri = "/*",
      .method = HTTP_PROPFIND,
      .handler = handle_webdav_propfind,
      .user_ctx = this
    },
    {
      .uri = "/*",
      .method = HTTP_PROPPATCH,
      .handler = handle_webdav_proppatch,
      .user_ctx = this
    },
    {
      .uri = "/*",
      .method = HTTP_MKCOL,
      .handler = handle_webdav_mkcol,
      .user_ctx = this
    },
    {
      .uri = "/*",
      .method = HTTP_COPY,
      .handler = handle_webdav_copy,
      .user_ctx = this
    },
    {
      .uri = "/*",
      .method = HTTP_MOVE,
      .handler = handle_webdav_move,
      .user_ctx = this
    },
    {
      .uri = "/*",
      .method = HTTP_OPTIONS,
      .handler = handle_webdav_options,
      .user_ctx = this
    }
  };

  for (const auto &handler : uri_handlers) {
    err = httpd_register_uri_handler(this->server_, &handler);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to register handler for %s (err: %d)", handler.uri, err);
    } else {
      ESP_LOGI(TAG, "Successfully registered handler for %s", handler.uri);
    }
  }
}

void WebDAVBox3::start_server() {
  if (this->server_ == nullptr) {
    ESP_LOGE(TAG, "Server not configured");
    return;
  }
  ESP_LOGI(TAG, "WebDAV server started successfully");
}

void WebDAVBox3::stop_server() {
  if (this->server_ != nullptr) {
    httpd_stop(this->server_);
    this->server_ = nullptr;
    ESP_LOGI(TAG, "WebDAV server stopped");
  }
}

std::string WebDAVBox3::get_file_path(httpd_req_t *req, const std::string &root_path) {
  std::string uri(req->uri);
  if (uri == "/") return root_path;
  return root_path + uri;
}

bool WebDAVBox3::authenticate(httpd_req_t *req) {
  if (!this->auth_enabled_) {
    return true;
  }

  char *auth_header = nullptr;
  size_t auth_header_len = httpd_req_get_hdr_value_len(req, "Authorization");
  
  if (auth_header_len > 0) {
    auth_header = (char*)malloc(auth_header_len + 1);
    if (httpd_req_get_hdr_value_str(req, "Authorization", auth_header, auth_header_len + 1) == ESP_OK) {
      std::string auth_str = this->username_ + ":" + this->password_;
      std::string expected_auth = "Basic " + esphome::base64_encode((const uint8_t*)auth_str.c_str(), auth_str.length());
      bool authenticated = (strcmp(auth_header, expected_auth.c_str()) == 0);
      free(auth_header);
      return authenticated;
    }
    free(auth_header);
  }
  
  return false;
}

esp_err_t WebDAVBox3::send_auth_required_response(httpd_req_t *req) {
  httpd_resp_set_status(req, "401 Unauthorized");
  httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"WebDAV Server\"");
  return httpd_resp_send(req, nullptr, 0);
}

esp_err_t WebDAVBox3::handle_webdav_options(httpd_req_t *req) {
  ESP_LOGI(TAG, "OPTIONS request received for: %s", req->uri);

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "OPTIONS,PROPFIND,GET,HEAD,PUT,DELETE,MKCOL,MOVE,COPY,PROPPATCH");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Authorization,Content-Type,Depth,Destination");
  httpd_resp_set_hdr(req, "DAV", "1,2");
  httpd_resp_set_hdr(req, "Allow", "OPTIONS,PROPFIND,GET,HEAD,PUT,DELETE,MKCOL,MOVE,COPY,PROPPATCH");
  httpd_resp_set_hdr(req, "Content-Length", "0");
  httpd_resp_set_hdr(req, "MS-Author-Via", "DAV");
  
  httpd_resp_set_type(req, "text/plain");
  
  return httpd_resp_send(req, nullptr, 0);
}

esp_err_t WebDAVBox3::handle_webdav_propfind(httpd_req_t *req) {
  ESP_LOGI(TAG, "PROPFIND request received for: %s", req->uri);
  
  std::string path = get_file_path(req, "/sdcard");
  struct stat st;
  
  if (stat(path.c_str(), &st) != 0) {
    ESP_LOGE(TAG, "Path not found: %s (errno: %d)", path.c_str(), errno);
    return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not found");
  }

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_type(req, "application/xml; charset=utf-8");
  httpd_resp_set_status(req, "207 Multi-Status");
  httpd_resp_set_hdr(req, "DAV", "1,2");

  std::stringstream xml;
  xml << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
      << "<D:multistatus xmlns:D=\"DAV:\">\n";

  // Add current resource
  xml << "<D:response>\n"
      << "  <D:href>" << req->uri << "</D:href>\n"
      << "  <D:propstat>\n"
      << "    <D:prop>\n"
      << "      <D:resourcetype>";
  
  if (S_ISDIR(st.st_mode)) {
    xml << "<D:collection/>";
  }
  
  xml << "</D:resourcetype>\n"
      << "      <D:getcontentlength>" << st.st_size << "</D:getcontentlength>\n"
      << "      <D:getlastmodified>" << ctime(&st.st_mtime) << "</D:getlastmodified>\n"
      << "    </D:prop>\n"
      << "    <D:status>HTTP/1.1 200 OK</D:status>\n"
      << "  </D:propstat>\n"
      << "</D:response>\n";

  if (S_ISDIR(st.st_mode)) {
    DIR *dir = opendir(path.c_str());
    if (dir != nullptr) {
      struct dirent *entry;
      while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;

        std::string fullPath = path + "/" + name;
        struct stat entry_st;
        if (stat(fullPath.c_str(), &entry_st) == 0) {
          std::string href = std::string(req->uri) + (req->uri[strlen(req->uri)-1] == '/' ? "" : "/") + name;
          
          xml << "<D:response>\n"
              << "  <D:href>" << href << "</D:href>\n"
              << "  <D:propstat>\n"
              << "    <D:prop>\n"
              << "      <D:resourcetype>";
          
          if (S_ISDIR(entry_st.st_mode)) {
            xml << "<D:collection/>";
          }
          
          xml << "</D:resourcetype>\n"
              << "      <D:getcontentlength>" << entry_st.st_size << "</D:getcontentlength>\n"
              << "      <D:getlastmodified>" << ctime(&entry_st.st_mtime) << "</D:getlastmodified>\n"
              << "    </D:prop>\n"
              << "    <D:status>HTTP/1.1 200 OK</D:status>\n"
              << "  </D:propstat>\n"
              << "</D:response>\n";
        }
      }
      closedir(dir);
    }
  }

  xml << "</D:multistatus>";
  
  return httpd_resp_send(req, xml.str().c_str(), xml.str().length());
}

esp_err_t WebDAVBox3::handle_webdav_get(httpd_req_t *req) {
  ESP_LOGI(TAG, "GET request received for: %s", req->uri);
  
  std::string path = get_file_path(req, "/sdcard");
  struct stat st;
  
  if (stat(path.c_str(), &st) != 0) {
    ESP_LOGE(TAG, "File not found: %s", path.c_str());
    return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
  }

  if (S_ISDIR(st.st_mode)) {
    return handle_webdav_propfind(req);
  }

  FILE* file = fopen(path.c_str(), "rb");
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file: %s", path.c_str());
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open file");
  }

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_type(req, "application/octet-stream");
  char contentLength[32];
  snprintf(contentLength, sizeof(contentLength), "%ld", st.st_size);
  httpd_resp_set_hdr(req, "Content-Length", contentLength);

  char buffer[1024];
  size_t bytesRead;
  while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
    if (httpd_resp_send_chunk(req, buffer, bytesRead) != ESP_OK) {
      fclose(file);
      return ESP_FAIL;
    }
  }

  fclose(file);
  return httpd_resp_send_chunk(req, nullptr, 0);
}

esp_err_t WebDAVBox3::handle_webdav_put(httpd_req_t *req) {
  ESP_LOGI(TAG, "PUT request received for: %s", req->uri);
  
  std::string path = get_file_path(req, "/sdcard");
  
  // Créer les répertoires parents si nécessaire
  size_t pos = 0;
  while ((pos = path.find('/', pos + 1)) != std::string::npos) {
    std::string dir = path.substr(0, pos);
    mkdir(dir.c_str(), 0755);
  }
  
  FILE* file = fopen(path.c_str(), "wb");
  if (!file) {
    ESP_LOGE(TAG, "Failed to create file: %s", path.c_str());
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
  }

  char buffer[1024];
  int remaining = req->content_len;
  
  while (remaining > 0) {
    int toRead = std::min(remaining, (int)sizeof(buffer));
    int received = httpd_req_recv(req, buffer, toRead);
    
    if (received <= 0) {
      fclose(file);
      ESP_LOGE(TAG, "Failed to receive data");
      return ESP_FAIL;
    }
    
    fwrite(buffer, 1, received, file);
    remaining -= received;
  }

  fclose(file);
  httpd_resp_set_status(req, "201 Created");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, nullptr, 0);
}

esp_err_t WebDAVBox3::handle_webdav_mkcol(httpd_req_t *req) {
  ESP_LOGI(TAG, "MKCOL request received for: %s", req->uri);
  
  std::string path = get_file_path(req, "/sdcard");
  
  // Créer les répertoires parents si nécessaire
  size_t pos = 0;
  while ((pos = path.find('/', pos + 1)) != std::string::npos) {
    std::string dir = path.substr(0, pos);
    mkdir(dir.c_str(), 0755);
  }
  
  if (mkdir(path.c_str(), 0755) != 0) {
    ESP_LOGE(TAG, "Failed to create directory: %s", path.c_str());
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create directory");
  }

  httpd_resp_set_status(req, "201 Created");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, nullptr, 0);
}

esp_err_t WebDAVBox3::handle_webdav_delete(httpd_req_t *req) {
  ESP_LOGI(TAG, "DELETE request received for: %s", req->uri);
  
  std::string path = get_file_path(req, "/sdcard");
  struct stat st;
  
  if (stat(path.c_str(), &st) != 0) {
    ESP_LOGE(TAG, "Path not found: %s", path.c_str());
    return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not found");
  }

  bool success;
  if (S_ISDIR(st.st_mode)) {
    success = (rmdir(path.c_str()) == 0);
  } else {
    success = (remove(path.c_str()) == 0);
  }

  if (!success) {
    ESP_LOGE(TAG, "Failed to delete: %s", path.c_str());
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to delete");
  }

  httpd_resp_set_status(req, "204 No Content");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, nullptr, 0);
}

esp_err_t WebDAVBox3::handle_webdav_move(httpd_req_t *req) {
  ESP_LOGI(TAG, "MOVE request received for: %s", req->uri);
  
  char *destination = nullptr;
  size_t dest_len = httpd_req_get_hdr_value_len(req, "Destination");
  
  if (dest_len <= 0) {
    ESP_LOGE(TAG, "Destination header missing");
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Destination header required");
  }

  destination = (char*)malloc(dest_len + 1);
  if (httpd_req_get_hdr_value_str(req, "Destination", destination, dest_len + 1) != ESP_OK) {
    free(destination);
    ESP_LOGE(TAG, "Invalid destination header");
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid destination");
  }

  std::string src_path = get_file_path(req, "/sdcard");
  std::string dest_path = get_file_path(req, "/sdcard");

  if (rename(src_path.c_str(), dest_path.c_str()) != 0) {
    ESP_LOGE(TAG, "Failed to move: %s to %s", src_path.c_str(), dest_path.c_str());
    free(destination);
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to move resource");
  }

  free(destination);
  httpd_resp_set_status(req, "201 Created");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, nullptr, 0);
}

esp_err_t WebDAVBox3::handle_webdav_copy(httpd_req_t *req) {
  ESP_LOGI(TAG, "COPY request received for: %s", req->uri);
  return handle_webdav_move(req);  // Pour l'instant, même comportement que MOVE
}

esp_err_t WebDAVBox3::handle_webdav_proppatch(httpd_req_t *req) {
  ESP_LOGI(TAG, "PROPPATCH request received for: %s", req->uri);
  
  httpd_resp_set_status(req, "207 Multi-Status");
  httpd_resp_set_type(req, "application/xml; charset=utf-8");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  
  const char* response = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                        "<D:multistatus xmlns:D=\"DAV:\">\n"
                        "  <D:response>\n"
                        "    <D:href>/</D:href>\n"
                        "    <D:propstat>\n"
                        "      <D:status>HTTP/1.1 200 OK</D:status>\n"
                        "    </D:propstat>\n"
                        "  </D:response>\n"
                        "</D:multistatus>";
  
  return httpd_resp_send(req, response, strlen(response));
}

}  // namespace webdavbox3
}  // namespace esphome




















