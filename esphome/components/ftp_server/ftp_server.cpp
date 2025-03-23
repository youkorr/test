#include "http_server.h"

void HttpServer::setup() {
  if (!SD.begin()) {
    ESP_LOGE("SD", "Échec d'initialisation de la carte SD");
    return;
  }
  
  ESP_LOGI("HTTP", "Serveur HTTP démarré sur le port 8080");

  // Route simple pour servir un fichier audio
  server.on("/audio", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("file")) {
      request->send(400, "text/plain", "Paramètre 'file' manquant");
      return;
    }

    String filename = "/" + request->getParam("file")->value();
    ESP_LOGI("HTTP", "Demande de fichier: %s", filename.c_str());

    // Lire le fichier avec ton composant `sd_card`
    auto fileData = id(sd_card)->read_file(filename.c_str());
    if (fileData.empty()) {
      request->send(404, "text/plain", "Fichier non trouvé ou vide");
      return;
    }

    // Détecter le type MIME
    String contentType = "audio/mpeg"; // Par défaut MP3
    if (filename.endsWith(".flac")) contentType = "audio/flac";
    else if (filename.endsWith(".wav")) contentType = "audio/wav";
    else if (filename.endsWith(".mp3")) contentType = "audio/mpeg";

    // Envoyer le fichier
    request->send_P(200, contentType.c_str(), fileData.data(), fileData.size());
  });

  server.begin();
}

}  // namespace ftp_server
}  // namespace esphome
















