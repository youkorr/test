#include "webdavbox3.h"
#include "esphome/core/log.h"
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <fstream>
#include <esp_http_server.h>

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

esp_err_t WebDAVBox3::handle_method_not_allowed(httpd_req_t *req) {
    ESP_LOGE(TAG, "Method not allowed: %d on URI %s", req->method, req->uri);
    httpd_resp_set_status(req, "405 Method Not Allowed");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

void WebDAVBox3::configure_http_server() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = port_;
  config.ctrl_port = port_ + 1000;
  config.max_uri_handlers = 20;
  
  if (httpd_start(&server_, &config) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start server on port %d", port_);
    server_ = nullptr;
    return;
  }

  // Catch-all handler for unsupported methods
  httpd_uri_t method_not_allowed_uri = {
    .uri = "/*",
    .method = HTTP_ANY,
    .handler = [](httpd_req_t *req) {
      return static_cast<WebDAVBox3*>(req->user_ctx)->handle_method_not_allowed(req);
    },
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &method_not_allowed_uri);

  // Root handler
  httpd_uri_t root_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = handle_root,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &root_uri);
  
  // WebDAV OPTIONS handler
  httpd_uri_t options_uri = {
    .uri = "/*",
    .method = HTTP_OPTIONS,
    .handler = handle_webdav_options,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &options_uri);
  
  // PROPFIND handlers
  httpd_uri_t propfind_uri = {
    .uri = "/",
    .method = HTTP_PROPFIND,
    .handler = handle_webdav_propfind,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &propfind_uri);
  
  httpd_uri_t propfind_wildcard_uri = {
    .uri = "/*",
    .method = HTTP_PROPFIND,
    .handler = handle_webdav_propfind,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &propfind_wildcard_uri);
  
  // Other WebDAV handlers
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
  
  // LOCK/UNLOCK handlers
  httpd_uri_t lock_uri = {
    .uri = "/*",
    .method = HTTP_LOCK,
    .handler = handle_webdav_lock,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &lock_uri);
  
  httpd_uri_t unlock_uri = {
    .uri = "/*",
    .method = HTTP_UNLOCK,
    .handler = handle_webdav_unlock,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &unlock_uri);
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

esp_err_t WebDAVBox3::handle_webdav_options(httpd_req_t *req) {
  const char* allowed_methods = "OPTIONS, GET, HEAD, PUT, DELETE, PROPFIND, MKCOL, COPY, MOVE, LOCK, UNLOCK";
  httpd_resp_set_hdr(req, "Allow", allowed_methods);
  httpd_resp_set_hdr(req, "DAV", "1, 2");
  httpd_resp_set_hdr(req, "MS-Author-Via", "DAV");
  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_lock(httpd_req_t *req) {
  ESP_LOGD(TAG, "LOCK sur %s", req->uri);
  
  std::string response = "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n"
                         "<D:prop xmlns:D=\"DAV:\">\n"
                         "  <D:lockdiscovery>\n"
                         "    <D:activelock>\n"
                         "      <D:locktype><D:write/></D:locktype>\n"
                         "      <D:lockscope><D:exclusive/></D:lockscope>\n"
                         "      <D:depth>0</D:depth>\n"
                         "      <D:timeout>Second-600</D:timeout>\n"
                         "    </D:activelock>\n"
                         "  </D:lockdiscovery>\n"
                         "</D:prop>";
  
  httpd_resp_set_type(req, "application/xml");
  httpd_resp_set_status(req, "200 OK");
  httpd_resp_send(req, response.c_str(), response.length());
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_unlock(httpd_req_t *req) {
  ESP_LOGD(TAG, "UNLOCK sur %s", req->uri);
  httpd_resp_set_status(req, "204 No Content");
  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}

std::string WebDAVBox3::get_file_path(httpd_req_t *req, const std::string &root_path) {
  std::string uri = req->uri;
  std::string path = root_path;
  
  if (path.back() != '/' && !uri.empty() && uri.front() != '/')
    path += '/';
  
  if (!uri.empty() && uri.front() == '/' && path.back() == '/')
    path += uri.substr(1);
  else
    path += uri;
  
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

std::string WebDAVBox3::generate_prop_xml(const std::string &href, bool is_directory, time_t modified, size_t size) {
  char time_buf[30];
  strftime(time_buf, sizeof(time_buf), "%Y-%m-%dT%H:%M:%SZ", gmtime(&modified));
  
  std::string xml = "  <D:response>\n";
  xml += "    <D:href>" + href + "</D:href>\n";
  xml += "    <D:propstat>\n";
  xml += "      <D:prop>\n";
  xml += "        <D:resourcetype>";
  if (is_directory) {
    xml += "<D:collection/>";
  }
  xml += "</D:resourcetype>\n";
  xml += "        <D:getlastmodified>" + std::string(time_buf) + "</D:getlastmodified>\n";
  if (!is_directory) {
    xml += "        <D:getcontentlength>" + std::to_string(size) + "</D:getcontentlength>\n";
  }
  xml += "      </D:prop>\n";
  xml += "      <D:status>HTTP/1.1 200 OK</D:status>\n";
  xml += "    </D:propstat>\n";
  xml += "  </D:response>\n";
  
  return xml;
}

esp_err_t WebDAVBox3::handle_root(httpd_req_t *req) {
  httpd_resp_send(req, "ESP32 WebDAV Server OK", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_propfind(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  std::string path = get_file_path(req, inst->root_path_);

  ESP_LOGD(TAG, "PROPFIND sur %s", path.c_str());
  
  struct stat st;
  if (stat(path.c_str(), &st) != 0) {
    ESP_LOGE(TAG, "Chemin non trouvé: %s", path.c_str());
    return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not Found");
  }
  
  bool is_directory = S_ISDIR(st.st_mode);
  std::string depth_header = "0";
  
  char depth_value[10];
  if (httpd_req_get_hdr_value_str(req, "Depth", depth_value, sizeof(depth_value)) == ESP_OK) {
    depth_header = depth_value;
  }
  
  std::string response = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                         "<D:multistatus xmlns:D=\"DAV:\">\n";
  
  std::string uri_path = req->uri;
  if (uri_path.empty() || uri_path == "/") uri_path = "/";
  if (is_directory && uri_path.back() != '/') uri_path += '/';
  
  response += generate_prop_xml(uri_path, is_directory, st.st_mtime, st.st_size);
  
  if (is_directory && (depth_header == "1" || depth_header == "infinity")) {
    for (const auto &file_name : list_dir(path)) {
      std::string file_path = path;
      if (file_path.back() != '/') file_path += '/';
      file_path += file_name;
      
      struct stat file_stat;
      if (stat(file_path.c_str(), &file_stat) == 0) {
        bool is_file_dir = S_ISDIR(file_stat.st_mode);
        std::string href = uri_path;
        if (href.back() != '/') href += '/';
        href += file_name;
        if (is_file_dir) href += '/';
        
        response += generate_prop_xml(href, is_file_dir, file_stat.st_mtime, file_stat.st_size);
      }
    }
  }
  
  response += "</D:multistatus>";
  
  httpd_resp_set_type(req, "application/xml; charset=utf-8");
  httpd_resp_set_status(req, "207 Multi-Status");
  httpd_resp_send(req, response.c_str(), response.length());
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_get(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  std::string path = get_file_path(req, inst->root_path_);

  ESP_LOGD(TAG, "GET %s (URI: %s)", path.c_str(), req->uri);
  
  if (is_dir(path)) {
    ESP_LOGD(TAG, "C'est un répertoire, redirection vers PROPFIND");
    return handle_webdav_propfind(req);
  }

  struct stat st;
  if (stat(path.c_str(), &st) != 0) {
    ESP_LOGE(TAG, "Fichier non trouvé: %s", path.c_str());
    return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
  }

  FILE *file = fopen(path.c_str(), "rb");
  if (!file) {
    ESP_LOGE(TAG, "Impossible d'ouvrir le fichier: %s", path.c_str());
    return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
  }

  fseek(file, 0, SEEK_END);
  size_t file_size = ftell(file);
  fseek(file, 0, SEEK_SET);
  
  httpd_resp_set_type(req, "application/octet-stream");
  httpd_resp_set_hdr(req, "Content-Length", std::to_string(file_size).c_str());

  char buffer[1024];
  size_t read_bytes;
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
  
  std::string parent_dir = path.substr(0, path.find_last_of('/'));
  if (!parent_dir.empty() && !is_dir(parent_dir)) {
    mkdir(parent_dir.c_str(), 0755);
  }

  FILE *file = fopen(path.c_str(), "wb");
  if (!file)
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");

  char buffer[1024];
  int received;

  while ((received = httpd_req_recv(req, buffer, sizeof(buffer))) > 0) {
    fwrite(buffer, 1, received, file);
  }

  fclose(file);
  httpd_resp_set_status(req, "201 Created");
  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_delete(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  std::string path = get_file_path(req, inst->root_path_);

  ESP_LOGD(TAG, "DELETE %s", path.c_str());
  
  if (is_dir(path)) {
    if (rmdir(path.c_str()) == 0) {
      httpd_resp_set_status(req, "204 No Content");
      httpd_resp_send(req, NULL, 0);
      return ESP_OK;
    }
  } else {
    if (remove(path.c_str()) == 0) {
      httpd_resp_set_status(req, "204 No Content");
      httpd_resp_send(req, NULL, 0);
      return ESP_OK;
    }
  }

  return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
}

esp_err_t WebDAVBox3::handle_webdav_mkcol(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  std::string path = get_file_path(req, inst->root_path_);

  ESP_LOGD(TAG, "MKCOL %s", path.c_str());
  
  struct stat st;
  if (stat(path.c_str(), &st) == 0) {
    return httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "Resource already exists");
  }

  if (mkdir(path.c_str(), 0755) == 0) {
    httpd_resp_set_status(req, "201 Created");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
  }

  return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create directory");
}

esp_err_t WebDAVBox3::handle_webdav_move(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  std::string src = get_file_path(req, inst->root_path_);

  char dest_uri[512];
  if (httpd_req_get_hdr_value_str(req, "Destination", dest_uri, sizeof(dest_uri)) == ESP_OK) {
    const char *path_start = strstr(dest_uri, inst->root_path_.c_str());
    std::string dst;
    
    if (path_start) {
      dst = path_start;
    } else {
      dst = inst->root_path_;
      if (dst.back() != '/' && dest_uri[0] != '/') dst += '/';
      if (dst.back() == '/' && dest_uri[0] == '/') dst += (dest_uri + 1);
      else dst += dest_uri;
    }
    
    ESP_LOGD(TAG, "MOVE de %s vers %s", src.c_str(), dst.c_str());
    
    std::string parent_dir = dst.substr(0, dst.find_last_of('/'));
    if (!parent_dir.empty() && !is_dir(parent_dir)) {
      mkdir(parent_dir.c_str(), 0755);
    }
    
    if (rename(src.c_str(), dst.c_str()) == 0) {
      httpd_resp_set_status(req, "201 Created");
      httpd_resp_send(req, NULL, 0);
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
    const char *path_start = strstr(dest_uri, inst->root_path_.c_str());
    std::string dst;
    
    if (path_start) {
      dst = path_start;
    } else {
      dst = inst->root_path_;
      if (dst.back() != '/' && dest_uri[0] != '/') dst += '/';
      if (dst.back() == '/' && dest_uri[0] == '/') dst += (dest_uri + 1);
      else dst += dest_uri;
    }
    
    ESP_LOGD(TAG, "COPY de %s vers %s", src.c_str(), dst.c_str());
    
    std::string parent_dir = dst.substr(0, dst.find_last_of('/'));
    if (!parent_dir.empty() && !is_dir(parent_dir)) {
      mkdir(parent_dir.c_str(), 0755);
    }
    
    if (is_dir(src)) {
      return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Directory copy not supported");
    }
    
    std::ifstream in(src, std::ios::binary);
    std::ofstream out(dst, std::ios::binary);

    if (!in || !out)
      return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Copy failed");

    out << in.rdbuf();
    
    httpd_resp_set_status(req, "201 Created");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
  }

  return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Copy failed");
}

}  // namespace webdavbox3
}  // namespace esphome




















