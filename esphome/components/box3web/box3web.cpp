#include "box3web.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace box3web {

static const char *TAG = "Box3Web";

Box3Web::Box3Web(web_server_base::WebServerBase *base) : base_(base) {}

void Box3Web::setup() { this->base_->add_handler(this); }

void Box3Web::dump_config() {
  ESP_LOGCONFIG(TAG, "Box3Web File Server:");
  ESP_LOGCONFIG(TAG, "  Address: %s:%u", network::get_use_address().c_str(), this->base_->get_port());
  ESP_LOGCONFIG(TAG, "  Url Prefix: %s", this->url_prefix_.c_str());
  ESP_LOGCONFIG(TAG, "  Root Path: %s", this->root_path_.c_str());
  ESP_LOGCONFIG(TAG, "  Deletion Enabled: %s", TRUEFALSE(this->deletion_enabled_));
  ESP_LOGCONFIG(TAG, "  Download Enabled : %s", TRUEFALSE(this->download_enabled_));
  ESP_LOGCONFIG(TAG, "  Upload Enabled : %s", TRUEFALSE(this->upload_enabled_));
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

  if (index == 0 && !this->sd_mmc_card_->is_directory(path)) {
    auto response = request->beginResponse(401, "application/json", "{ \"error\": \"invalid upload folder\" }");
    response->addHeader("Connection", "close");
    request->send(response);
    return;
  }
  std::string file_name(filename.c_str());
  std::string full_path = Path::join(path, file_name);

  if (index == 0) {
    ESP_LOGD(TAG, "uploading file %s to %s", file_name.c_str(), path.c_str());
    if (!this->sd_mmc_card_->write_file(full_path.c_str(), data, len, true)) { // overwrite = true pour la première écriture
      request->send(500, "application/json", "{ \"error\": \"failed to create file\" }");
      return;
    }
  } else {
    this->sd_mmc_card_->append_file(full_path.c_str(), data, len);
  }

  if (final) {
    auto response = request->beginResponse(201, "text/html", "upload success");
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
  std::string file_type = get_file_type(file_name);
  uint64_t file_size = info.size;

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
  response->print(file_type.c_str());
  response->print("</td><td>");
  response->print(format_size(file_size).c_str());
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
  response->print(F("<!DOCTYPE html><html lang=\"en\"><head><meta charset=UTF-8><meta "
                    "name=viewport content=\"width=device-width, initial-scale=1,user-scalable=no\">"
                    "</head><body>"
                    "<h1>SD Card Content</h1><h2>Folder "));

  response->print(path.c_str());
  response->print(F("</h2>"));
  if (this->upload_enabled_)
    response->print(F("<form method=\"POST\" enctype=\"multipart/form-data\">"
                      "<input type=\"file\" name=\"file\"><input type=\"submit\" value=\"upload\"></form>"));
  response->print(F("<a href=\"/"));
  response->print(this->url_prefix_.c_str());
  response->print(F("\">Home</a></br></br><table id=\"files\"><thead><tr><th>Name<th>Type<th>Size<th>Actions<tbody>"));
  auto entries = this->sd_mmc_card_->list_directory_file_info(path, 0);
  for (auto const &entry : entries)
    write_row(response, entry);

  response->print(F("</tbody></table>"
                    "<script>"
                    "function navigate_to(path) { window.location.href = path; }"
                    "function delete_file(path) {fetch(path, {method: \"DELETE\"});}"
                    "function download_file(path, filename) {"
                    "fetch(path).then(response => response.blob())"
                    ".then(blob => {"
                    "const link = document.createElement('a');"
                    "link.href = URL.createObjectURL(blob);"
                    "link.download = filename;"
                    "link.click();"
                    "}).catch(console.error);"
                    "} "
                    "</script>"
                    "</body></html>"));

  request->send(response);
}

void Box3Web::handle_download(AsyncWebServerRequest *request, std::string const &path) const {
  if (!this->download_enabled_) {
    request->send(401, "application/json", "{ \"error\": \"file download is disabled\" }");
    return;
  }

  size_t fileSize = this->sd_mmc_card_->file_size(path);

  AsyncWebServerResponse *response = request->beginResponse(
    [this, path](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
      return this->sd_mmc_card_->read_file_block(path, buffer, maxLen, index);
    },
    fileSize,
    "application/octet-stream"
  );

  if (response == nullptr) {
    request->send(500, "text/plain", "Failed to create response");
    return;
  }

  response->setContentDisposition("attachment; filename=\"" + Path::file_name(path).c_str() + "\"");
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

std::string Path::file_name(std::string const &path) {
  size_t pos = path.rfind(Path::separator);
  if (pos != std::string::npos) {
    return path.substr(pos + 1);
  }
  return "";
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







