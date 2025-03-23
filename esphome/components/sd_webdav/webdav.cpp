#include "webdav.h"

#include <SPIFFS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include <esphome/core/log.h>
#include <esphome/components/sdcard/sdcard.h>

ESPHOME_LOG_NAMESPACE_BEGIN

namespace webdav {

void WebDAV::setup() {
  // Vérifier que la carte SD est montée
  if (!this->sd_card_->is_mounted()) {
    ESP_LOGE("WebDAV", "SD card not mounted");
    return;
  }

  // Initialiser le serveur WebDAV
  this->server = new AsyncWebServer(80);
  this->setup_routes();
  this->server->begin();
}

void WebDAV::setup_routes() {
  // Authentification BASIC
  auto auth_handler = [&]() {
    if (!this->username_[0] && !this->password_[0]) {
      return;
    }
    if (!this->server->authenticate(this->username_, this->password_)) {
      this->server->request()->authenticate(this->username_, this->password_);
    }
  };

  // Route de base pour /sdcard/*
  this->server->on(this->mount_point_ + "/*", HTTP_ANY, [this, auth_handler](AsyncWebServerRequest *request) {
    auth_handler();

    // Ignorer les requêtes non WebDAV
    if (!request->isMethod(HTTP_PROPFIND) && !request->isMethod(HTTP_PUT) &&
        !request->isMethod(HTTP_DELETE) && !request->isMethod(HTTP_MKCOL)) {
      request->send(405, "text/plain", "Method not allowed");
      return;
    }

    // Traiter la requête
    if (request->method() == HTTP_PROPFIND) {
      this->handle_propfind(request);
    } else if (request->method() == HTTP_PUT) {
      this->handle_put(request);
    } else if (request->method() == HTTP_DELETE) {
      this->handle_delete(request);
    } else if (request->method() == HTTP_MKCOL) {
      this->handle_mkdir(request);
    }
  });
}

void WebDAV::handle_propfind(AsyncWebServerRequest *request) {
  // Extraction du chemin relatif
  String path = request->url().substring(strlen(this->mount_point_));
  if (path == "/") path = "";
  String full_path = "/" + String(this->mount_point_) + path;

  // Vérifier l'existence du fichier/dossier
  if (!this->sd_card_->exists(full_path.c_str())) {
    request->send(404, "text/plain", "File not found");
    return;
  }

  // Renvoyer les métadonnées (ex: liste de fichiers)
  if (this->sd_card_->isDirectory(full_path.c_str())) {
    // Renvoyer une réponse XML WebDAV
    String response = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<D:multistatus xmlns:D=\"DAV:\">\n";
    response += "<D:response>\n<D:href>" + path + "</D:href>\n<D:propstat>\n<D:prop>\n<D:resourcetype><D:collection/></D:resourcetype>\n</D:prop>\n<D:status>HTTP/1.1 200 OK</D:status>\n</D:propstat>\n</D:response>\n</D:multistatus>";

    request->send(207, "application/xml", response);
  } else {
    // Fichier simple
    response = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<D:multistatus xmlns:D=\"DAV:\">\n<D:response>\n<D:href>" + path + "</D:href>\n<D:propstat>\n<D:prop/>\n<D:status>HTTP/1.1 200 OK</D:status>\n</D:propstat>\n</D:response>\n</D:multistatus>";
    request->send(207, "application/xml", response);
  }
}

void WebDAV::handle_put(AsyncWebServerRequest *request) {
  String path = request->url().substring(strlen(this->mount_point_));
  if (path == "/") path = "";
  String full_path = "/" + String(this->mount_point_) + path;

  // Créer le fichier
  File file = this->sd_card_->open(full_path.c_str(), "w");
  if (!file) {
    request->send(500, "text/plain", "Could not create file");
    return;
  }

  // Écrire les données
  request->parseBody();
  if (request->hasParam("body")) {
    String body = request->getParam("body")->value();
    file.print(body);
    file.close();
    request->send(201, "text/plain", "File created");
  } else {
    file.close();
    request->send(400, "text/plain", "No body provided");
  }
}

void WebDAV::handle_delete(AsyncWebServerRequest *request) {
  String path = request->url().substring(strlen(this->mount_point_));
  if (path == "/") path = "";
  String full_path = "/" + String(this->mount_point_) + path;

  if (this->sd_card_->exists(full_path.c_str())) {
    if (this->sd_card_->isDirectory(full_path.c_str())) {
      this->sd_card_->rmdir(full_path.c_str());
    } else {
      this->sd_card_->remove(full_path.c_str());
    }
    request->send(204, "text/plain", "Deleted");
  } else {
    request->send(404, "text/plain", "File not found");
  }
}

void WebDAV::handle_mkdir(AsyncWebServerRequest *request) {
  String path = request->url().substring(strlen(this->mount_point_));
  if (path == "/") path = "";
  String full_path = "/" + String(this->mount_point_) + path;

  if (!this->sd_card_->exists(full_path.c_str())) {
    if (this->sd_card_->mkdir(full_path.c_str())) {
      request->send(201, "text/plain", "Directory created");
    } else {
      request->send(500, "text/plain", "Could not create directory");
    }
  } else {
    request->send(409, "text/plain", "Directory already exists");
  }
}

void WebDAV::loop() {
  // Rien à faire ici pour le moment
}

}  // namespace webdav

ESPHOME_LOG_NAMESPACE_END
