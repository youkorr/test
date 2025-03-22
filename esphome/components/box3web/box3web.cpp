#include "box3web.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace box3web {

static const char *TAG = "box3web";

Box3Web::Box3Web(web_server_base::WebServerBase *base) : base_(base) {}

void Box3Web::setup() {
  this->base_->add_handler(this);
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
  return str_startswith(std::string(request->url().c_str()), this->build_prefix());
}

void Box3Web::handleRequest(AsyncWebServerRequest *request) {
  if (str_startswith(std::string(request->url().c_str()), this->build_prefix())) {
    if (request->method() == HTTP_GET) {
      this->handle_get(request);
      return;
    }
    if (request->method() == HTTP_DELETE) {
      this->handle_delete(request);
      return;
    }
    // Gérer les requêtes POST pour les uploads
    if (request->method() == HTTP_POST && request->hasHeader("Content-Type")) {
      AsyncWebHeader* content_type_header = request->getHeader("Content-Type");
      if (content_type_header && strstr(content_type_header->value().c_str(), "multipart/form-data") != nullptr) {
        for (size_t i = 0; i < request->args(); i++) {
          AsyncWebParameter* param = request->getParam(i);
          if (param->isFile()) {
            std::string file_name = param->name().c_str();
            uint8_t* data = (uint8_t*)param->value().c_str();
            size_t len = param->value().length();
            this->handleUpload(request, file_name.c_str(), 0, data, len, true);
          }
        }
        return;
      }
    }
  }
}

void Box3Web::handleUpload(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data,
                           size_t len, bool final) {
  if (!this->upload_enabled_) {
    request->send(401, "application/json", "{ \"error\": \"file upload is disabled\" }");
    return;
  }

  std::string extracted = this->extract_path_from_url(std::string(request->url().c_str()));
  std::string path = this->build_absolute_path(extracted);

  // Vérifier si le chemin est un dossier valide
  if (index == 0 && !this->sd_mmc_card_->is_directory(path)) {
    auto response = request->beginResponse(401, "application/json", "{ \"error\": \"invalid upload folder\" }");
    response->addHeader("Connection", "close");
    request->send(response);
    return;
  }

  std::string file_name(filename.c_str());
  std::string full_file_path = Path::join(path, file_name);

  if (index == 0) {
    ESP_LOGD(TAG, "Uploading file %s to %s", file_name.c_str(), path.c_str());
    if (!this->sd_mmc_card_->write_file(full_file_path.c_str(), data, len)) {
      auto response = request->beginResponse(500, "application/json", "{ \"error\": \"failed to write file\" }");
      response->addHeader("Connection", "close");
      request->send(response);
      return;
    }
  } else {
    if (!this->sd_mmc_card_->append_file(full_file_path.c_str(), data, len)) {
      auto response = request->beginResponse(500, "application/json", "{ \"error\": \"failed to append to file\" }");
      response->addHeader("Connection", "close");
      request->send(response);
      return;
    }
  }

  if (final) {
    auto response = request->beginResponse(201, "text/html", "Upload success");
    response->addHeader("Connection", "close");
    request->send(response);
    return;
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

  if (!this->sd_mmc_card_->is_directory(path)) {
    handle_download(request, path);
    return;
  }

  handle_index(request, path);
}

std::string Box3Web::get_file_type(const std::string &file_name) const {
  size_t pos = file_name.rfind('.');
  if (pos == std::string::npos)
    return "Unknown";

  std::string ext = file_name.substr(pos + 1);
  if (ext == "flac" || ext == "mp3" || ext == "wav") {
    return "Audio";
  } else if (ext == "jpg" || ext == "jpeg" || ext == "png" || ext == "gif") {
    return "Image";
  } else if (ext == "pdf" || ext == "docx" || ext == "txt") {
    return "Document";
  } else if (ext == "yaml" || ext == "json") {
    return "Configuration";
  } else {
    return "Other";
  }
}

std::string Box3Web::format_size(uint64_t size) const {
  if (size < 1024)
    return std::to_string(size) + " B";
  else if (size < 1024 * 1024)
    return std::to_string(size / 1024) + " KB";
  else if (size < 1024 * 1024 * 1024)
    return std::to_string(size / (1024 * 1024)) + " MB";
  else
    return std::to_string(size / (1024 * 1024 * 1024)) + " GB";
}

void Box3Web::write_row(AsyncResponseStream *response, sd_mmc_card::FileInfo const &info) const {
  std::string uri = "/" + Path::join(this->url_prefix_, Path::remove_root_path(info.path, this->root_path_));
  std::string file_name = Path::file_name(info.path);
  std::string file_type = this->get_file_type(file_name); // Déterminer le type de fichier
  uint64_t file_size = info.size; // Taille du fichier

  response->print("<tr><td>");
  if (info.is_directory) {
    response->print("<button onClick=\"navigate_to('");
    response->print(uri.c_str());
    response->print("')\">");
    response->print(file_name.c_str());
    response->print("</button>");
  } else {
    response->print(file_name.c_str());
  }
  response->print("</td><td>");
  response->print(file_type.c_str()); // Afficher le type de fichier
  response->print("</td><td>");
  response->print(this->format_size(file_size).c_str()); // Afficher la taille du fichier
  response->print("</td><td>");
  if (!info.is_directory) {
    if (this->download_enabled_) {
      response->print("<button onClick=\"download_file('");
      response->print(uri.c_str());
      response->print("','");
      response->print(file_name.c_str());
      response->print("')\">Download</button>");
    }
    if (this->deletion_enabled_) {
      response->print("<button onClick=\"delete_file('");
      response->print(uri.c_str());
      response->print("')\">Delete</button>");
    }
  }
  response->print("</td></tr>");
}

void Box3Web::handle_index(AsyncWebServerRequest *request, std::string const &path) const {
  AsyncResponseStream *response = request->beginResponseStream("text/html");
  response->print(F(
      "<!DOCTYPE html>"
      "<html lang=\"en\">"
      "<head>"
      "<meta charset=\"UTF-8\">"
      "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\">"
      "</head>"
      "<body>"
      "<h1>SD Card Content</h1>"
      "<h2>Folder "));
  response->print(path.c_str());
  response->print(F("</h2>"));

  if (this->upload_enabled_) {
    response->print(F(
        "<form method=\"POST\" enctype=\"multipart/form-data\">"
        "<input type=\"file\" name=\"file\">"
        "<input type=\"submit\" value=\"Upload\">"
        "</form>"));
  }

  response->print(F("<a href=\"/"));
  response->print(this->url_prefix_.c_str());
  response->print(F("\">Home</a></br></br>"
                    "<table id=\"files\">"
                    "<thead><tr><th>Name<th>Type<th>Size<th>Actions</tr></thead>"
                    "<tbody>"));

  auto entries = this->sd_mmc_card_->list_directory_file_info(path, 0);
  for (auto const &entry : entries) {
    write_row(response, entry);
  }

  response->print(F("</tbody></table>"
                    "<script>"
                    "function navigate_to(path) { window.location.href = path; }"
                    "function delete_file(path) { fetch(path, { method: 'DELETE' }); }"
                    "function download_file(path, filename) {"
                    "fetch(path)"
                    ".then(response => response.blob())"
                    ".then(blob => {"
                    "const link = document.createElement('a');"
                    "link.href = URL.createObjectURL(blob);"
                    "link.download = filename;"
                    "link.click();"
                    "})"
                    ".catch(console.error);"
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

  // Lire le fichier depuis la carte SD/MMC
  auto file = this->sd_mmc_card_->read_file(path);
  if (file.size() == 0) {
    request->send(404, "application/json", "{ \"error\": \"file not found\" }");
    return;
  }

  // Extraire le nom du fichier pour l'en-tête Content-Disposition
  std::string file_name = Path::file_name(path);

  // Créer une réponse avec le contenu du fichier
#ifdef USE_ESP_IDF
  auto *response = request->beginResponse_P(200, "application/octet-stream", file.data(), file.size());
#else
  auto *response = request->beginResponseStream("application/octet-stream", file.size());
  response->write(file.data(), file.size());
#endif

  // Ajouter l'en-tête Content-Disposition pour forcer le téléchargement
  response->addHeader("Content-Disposition", ("attachment; filename=\"" + file_name + "\"").c_str());

  // Envoyer la réponse
  request->send(response);
}

void Box3Web::handle_delete(AsyncWebServerRequest *request) {
  if (!this->deletion_enabled_) {
    request->send(401, "application/json", "{ \"error\": \"file deletion is disabled\" }");
    return;
  }
  std::string extracted = this->extract_path_from_url(std::string(request->url().c_str()));
  std::string path = this->build_absolute_path(extracted);
  if (this->sd_mmc_card_->is_directory(path)) {
    request->send(401, "application/json", "{ \"error\": \"cannot delete a directory\" }");
    return;
  }
  if (this->sd_mmc_card_->delete_file(path)) {
    request->send(204, "application/json", "{}");
    return;
  }
  request->send(401, "application/json", "{ \"error\": \"failed to delete file\" }");
}

std::string Box3Web::build_prefix() const {
  if (this->url_prefix_.length() == 0 || this->url_prefix_.at(0) != '/')
    return "/" + this->url_prefix_;
  return this->url_prefix_;
}

std::string Box3Web::extract_path_from_url(std::string const &url) const {
  std::string prefix = this->build_prefix();
  return url.substr(prefix.size(), url.size() - prefix.size());
}

std::string Box3Web::build_absolute_path(std::string relative_path) const {
  if (relative_path.size() == 0)
    return this->root_path_;

  std::string absolute = Path::join(this->root_path_, relative_path);
  return absolute;
}

}  // namespace box3web
}  // namespace esphome






