#include "box3web.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"
#include "esphome/core/helpers.h"
#include "FS.h"
#include "SD_MMC.h"

namespace esphome {
namespace box3web {

static const char *TAG = "box3web";

Box3Web::Box3Web(web_server_base::WebServerBase *base) : base_(base) {}

void Box3Web::setup() { 
  this->base_->add_handler(this);
  // Enregistrer gestionnaire spécifique pour les POST (uploads)
  this->base_->get_server()->onFileUpload([this](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
    this->handleUpload(request, filename, index, data, len, final);
  });
}

void Box3Web::dump_config() {
  ESP_LOGCONFIG(TAG, "Box3Web File Server:");
  ESP_LOGCONFIG(TAG, "  Address: %s:%u", network::get_use_address().c_str(), this->base_->get_port());
  ESP_LOGCONFIG(TAG, "  Url Prefix: %s", this->url_prefix_.c_str());
  ESP_LOGCONFIG(TAG, "  Root Path: %s", this->root_path_.c_str());
  ESP_LOGCONFIG(TAG, "  Deletion Enabled: %s", TRUEFALSE(this->deletion_enabled_));
  ESP_LOGCONFIG(TAG, "  Download Enabled: %s", TRUEFALSE(this->download_enabled_));
  ESP_LOGCONFIG(TAG, "  Upload Enabled: %s", TRUEFALSE(this->upload_enabled_));
}

bool Box3Web::canHandle(AsyncWebServerRequest *request) {
  std::string url = request->url().c_str();
  bool can_handle = str_startswith(url, this->build_prefix());
  ESP_LOGD(TAG, "can handle %s: %s", url.c_str(), TRUEFALSE(can_handle));
  return can_handle;
}

void Box3Web::handleRequest(AsyncWebServerRequest *request) {
  ESP_LOGD(TAG, "Handling request: %s (method: %d)", request->url().c_str(), request->method());
  
  if (str_startswith(std::string(request->url().c_str()), this->build_prefix())) {
    if (request->method() == HTTP_GET) {
      this->handle_get(request);
      return;
    }
    else if (request->method() == HTTP_DELETE) {
      this->handle_delete(request);
      return;
    }
    else if (request->method() == HTTP_POST) {
      // Le téléchargement sera géré par handleUpload
      request->onDisconnect([](){
        ESP_LOGD(TAG, "Client disconnected after upload");
      });
      return;
    }
  }
  
  request->send(404, "text/plain", "Not found");
}

void Box3Web::handleUpload(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data,
                           size_t len, bool final) {
  if (!this->upload_enabled_) {
    request->send(401, "application/json", "{ \"error\": \"file upload is disabled\" }");
    return;
  }
  
  std::string extracted = this->extract_path_from_url(std::string(request->url().c_str()));
  std::string path = this->build_absolute_path(extracted);
  
  ESP_LOGD(TAG, "Handle upload: %s to path %s (index: %d, len: %d, final: %s)", 
           filename.c_str(), path.c_str(), index, len, TRUEFALSE(final));

  if (index == 0 && !this->sd_mmc_card_->is_directory(path)) {
    ESP_LOGW(TAG, "Invalid upload folder: %s", path.c_str());
    auto response = request->beginResponse(401, "application/json", "{ \"error\": \"invalid upload folder\" }");
    response->addHeader("Connection", "close");
    request->send(response);
    return;
  }
  
  std::string file_name(filename.c_str());
  std::string full_path = Path::join(path, file_name);
  
  if (index == 0) {
    ESP_LOGD(TAG, "Starting upload of file %s to %s", file_name.c_str(), path.c_str());
    if (!this->sd_mmc_card_->write_file(full_path.c_str(), data, len)) {
      ESP_LOGW(TAG, "Failed to start writing file: %s", full_path.c_str());
      return;
    }
  } else {
    if (!this->sd_mmc_card_->append_file(full_path.c_str(), data, len)) {
      ESP_LOGW(TAG, "Failed to append to file: %s", full_path.c_str());
      return;
    }
  }
  
  if (final) {
    ESP_LOGI(TAG, "Upload complete: %s (%d bytes)", full_path.c_str(), index + len);
    auto response = request->beginResponse(200, "text/html", 
      "<html><body><h1>Upload successful</h1><a href=\"/" + String(this->url_prefix_.c_str()) + 
      "\">Return to file list</a></body></html>");
    response->addHeader("Connection", "close");
    request->send(response);
  }
}

void Box3Web::set_url_prefix(std::string const &prefix) { this->url_prefix_ = prefix; }

void Box3Web::set_root_path(std::string const &path) { this->root_path_ = path; }

void Box3Web::set_sd_mmc_card(sd_mmc_card::SdMmc *card) { this->sd_mmc_card_ = card; }

void Box3Web::set_deletion_enabled(bool allow) { this->deletion_enabled_ = allow; }

void Box3Web::set_download_enabled(bool allow) { this->download_enabled_ = allow; }

void Box3Web::set_upload_enabled(bool allow) { this->upload_enabled_ = allow; }

void Box3Web::handle_get(AsyncWebServerRequest *request) const {
  std::string extracted = this->extract_path_from_url(std::string(request->url().c_str()));
  std::string path = this->build_absolute_path(extracted);
  
  ESP_LOGD(TAG, "GET request for path: %s (extracted: %s)", path.c_str(), extracted.c_str());

  if (!this->sd_mmc_card_->exists(path)) {
    request->send(404, "text/plain", "File not found");
    return;
  }

  if (!this->sd_mmc_card_->is_directory(path)) {
    handle_download(request, path);
    return;
  }

  handle_index(request, path);
}

void Box3Web::write_row(AsyncResponseStream *response, sd_mmc_card::FileInfo const &info) const {
  std::string uri = "/" + Path::join(this->url_prefix_, Path::remove_root_path(info.path, this->root_path_));
  std::string file_name = Path::file_name(info.path);
  
  if (file_name.empty()) {
    return; // Skip empty filenames
  }
  
  response->print("<tr><td>");
  if (info.is_directory) {
    response->print("<a href=\"");
    response->print(uri.c_str());
    response->print("\">");
    response->print(file_name.c_str());
    response->print("/</a>");
  } else {
    response->print(file_name.c_str());
    response->print(" (");
    response->print(info.size);
    response->print(" bytes)");
  }
  response->print("</td><td>");
  if (!info.is_directory) {
    if (this->download_enabled_) {
      response->print("<a href=\"");
      response->print(uri.c_str());
      response->print("\" download=\"");
      response->print(file_name.c_str());
      response->print("\">Download</a> ");
    }
    if (this->deletion_enabled_) {
      response->print("<button onclick=\"deleteFile('");
      response->print(uri.c_str());
      response->print("')\">Delete</button>");
    }
  }
  response->print("</td></tr>");
}

void Box3Web::handle_index(AsyncWebServerRequest *request, std::string const &path) const {
  AsyncResponseStream *response = request->beginResponseStream("text/html");
  response->print(F("<!DOCTYPE html><html lang=\"en\"><head><meta charset=UTF-8><meta "
                    "name=viewport content=\"width=device-width, initial-scale=1,user-scalable=no\">"
                    "<title>SD Card Content</title>"
                    "<style>"
                    "body {font-family: Arial, sans-serif; max-width: 800px; margin: 0 auto; padding: 10px;}"
                    "h1, h2 {color: #333;}"
                    "table {width: 100%; border-collapse: collapse;}"
                    "th, td {padding: 8px; text-align: left; border-bottom: 1px solid #ddd;}"
                    "button, input[type=submit] {background-color: #4CAF50; color: white; padding: 5px 10px; border: none; cursor: pointer;}"
                    "button:hover, input[type=submit]:hover {background-color: #45a049;}"
                    ".delete {background-color: #f44336;}"
                    ".delete:hover {background-color: #da190b;}"
                    "</style>"
                    "</head><body>"
                    "<h1>SD Card Content</h1><h2>Folder: "));

  response->print(Path::remove_root_path(path, this->root_path_).c_str());
  response->print(F("</h2>"));

  // Path navigation
  response->print(F("<div><a href=\"/"));
  response->print(this->url_prefix_.c_str());
  response->print(F("\">Home</a> | "));
  
  // Upload form
  if (this->upload_enabled_) {
    response->print(F("<form method=\"POST\" action=\"\" enctype=\"multipart/form-data\" "
                      "style=\"margin: 20px 0;\">"
                      "<input type=\"file\" name=\"file\">"
                      "<input type=\"submit\" value=\"Upload\">"
                      "</form>"));
  }
  
  response->print(F("</div><table id=\"files\"><thead><tr><th>Name<th>Actions<tbody>"));

  // Lister les fichiers
  auto entries = this->sd_mmc_card_->list_directory_file_info(path, 0);
  for (auto const &entry : entries) {
    write_row(response, entry);
  }

  response->print(F("</tbody></table>"
                    "<script>"
                    "function deleteFile(path) {"
                    "  if (confirm('Are you sure you want to delete this file?')) {"
                    "    fetch(path, {method: 'DELETE'})"
                    "    .then(response => {"
                    "      if (response.ok) {"
                    "        alert('File deleted successfully');"
                    "        location.reload();"
                    "      } else {"
                    "        alert('Failed to delete file');"
                    "      }"
                    "    })"
                    "    .catch(error => {"
                    "      alert('Error: ' + error);"
                    "    });"
                    "  }"
                    "}"
                    "</script>"
                    "</body></html>"));

  request->send(response);
}

void Box3Web::handle_download(AsyncWebServerRequest *request, std::string const &path) const {
  if (!this->download_enabled_) {
    request->send(401, "application/json", "{ \"error\": \"file download is disabled\" }");
    return;
  }

  // Utiliser un accès direct au fichier plutôt que de le charger en mémoire
  ESP_LOGD(TAG, "Starting download of file: %s", path.c_str());
  
  if (!this->sd_mmc_card_->exists(path)) {
    request->send(404, "text/plain", "File not found");
    return;
  }

  size_t fileSize = this->sd_mmc_card_->get_file_size(path);
  if (fileSize == 0) {
    request->send(404, "text/plain", "Empty file or error");
    return;
  }

  // Déterminer le type MIME en fonction de l'extension
  String contentType = "application/octet-stream";
  if (path.ends_with(".html") || path.ends_with(".htm")) contentType = "text/html";
  else if (path.ends_with(".css")) contentType = "text/css";
  else if (path.ends_with(".js")) contentType = "application/javascript";
  else if (path.ends_with(".json")) contentType = "application/json";
  else if (path.ends_with(".png")) contentType = "image/png";
  else if (path.ends_with(".jpg") || path.ends_with(".jpeg")) contentType = "image/jpeg";
  else if (path.ends_with(".ico")) contentType = "image/x-icon";
  else if (path.ends_with(".txt")) contentType = "text/plain";
  
  ESP_LOGD(TAG, "Sending file %s (%d bytes) with content type %s", 
           path.c_str(), fileSize, contentType.c_str());

#ifdef USE_ESP_IDF
  // Pour ESP-IDF, utiliser le streaming de fichier si possible
  request->send(this->sd_mmc_card_->get_filesystem(), path.c_str(), contentType);
#else
  // Fallback pour les autres plateformes
  auto file = this->sd_mmc_card_->read_file(path);
  if (file.size() == 0) {
    request->send(404, "application/json", "{ \"error\": \"failed to read file\" }");
    return;
  }
  auto *response = request->beginResponse_P(200, contentType, file.data(), file.size());
  response->addHeader("Content-Disposition", "attachment; filename=\"" + String(Path::file_name(path).c_str()) + "\"");
  request->send(response);
#endif
}

void Box3Web::handle_delete(AsyncWebServerRequest *request) {
  if (!this->deletion_enabled_) {
    request->send(401, "application/json", "{ \"error\": \"file deletion is disabled\" }");
    return;
  }
  
  std::string extracted = this->extract_path_from_url(std::string(request->url().c_str()));
  std::string path = this->build_absolute_path(extracted);
  
  ESP_LOGD(TAG, "DELETE request for: %s", path.c_str());
  
  if (!this->sd_mmc_card_->exists(path)) {
    request->send(404, "application/json", "{ \"error\": \"file not found\" }");
    return;
  }
  
  if (this->sd_mmc_card_->is_directory(path)) {
    request->send(401, "application/json", "{ \"error\": \"cannot delete a directory\" }");
    return;
  }
  
  if (this->sd_mmc_card_->delete_file(path)) {
    ESP_LOGI(TAG, "File deleted: %s", path.c_str());
    request->send(200, "application/json", "{ \"success\": true }");
    return;
  }
  
  ESP_LOGW(TAG, "Failed to delete file: %s", path.c_str());
  request->send(500, "application/json", "{ \"error\": \"failed to delete file\" }");
}

std::string Box3Web::build_prefix() const {
  if (this->url_prefix_.length() == 0 || this->url_prefix_.at(0) != '/')
    return "/" + this->url_prefix_;
  return this->url_prefix_;
}

std::string Box3Web::extract_path_from_url(std::string const &url) const {
  std::string prefix = this->build_prefix();
  if (url.size() < prefix.size()) {
    return "";
  }
  return url.substr(prefix.size(), url.size() - prefix.size());
}

std::string Box3Web::build_absolute_path(std::string relative_path) const {
  if (relative_path.size() == 0)
    return this->root_path_;

  std::string absolute = Path::join(this->root_path_, relative_path);
  return absolute;
}

std::string Path::file_name(std::string const &path) {
  size_t pos = path.rfind(Path::separator);
  if (pos != std::string::npos) {
    return path.substr(pos + 1);
  }
  return path; // Retourner le chemin complet si pas de séparateur
}

bool Path::is_absolute(std::string const &path) { return path.size() && path[0] == separator; }

bool Path::trailing_slash(std::string const &path) { return path.size() && path[path.length() - 1] == separator; }

std::string Path::join(std::string const &first, std::string const &second) {
  std::string result = first;
  if (!trailing_slash(first) && !is_absolute(second)) {
    result.push_back(separator);
  }
  if (trailing_slash(first) && is_absolute(second)) {
    result.pop_back();
  }
  result.append(second);
  return result;
}

std::string Path::remove_root_path(std::string path, std::string const &root) {
  if (!str_startswith(path, root))
    return path;
  if (path.size() == root.size() || path.size() < 2)
    return "/";
  return path.erase(0, root.size());
}

}  // namespace box3web
}  // namespace esphome














