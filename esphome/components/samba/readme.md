esphome:
  name: esp32_file_server
  platform: ESP32
  board: esp32dev

# Activer le mode DIO pour le Flash
esp32:
  flash_mode: dio

# Configuration WiFi
wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password
  
  # Optionnel - Point d'accès de secours
  ap:
    ssid: "ESP32 File Server"
    password: "fileserver"

# Activez l'API pour OTA
api:
  password: !secret api_password

# Activer OTA
ota:
  password: !secret ota_password

# Serveur Web
web_server:
  port: 80
  
# Logger
logger:
  level: DEBUG

# Configurer la carte SD
sd_mmc_card:
  id: my_sd_card
  cs_pin: GPIO5
  clk_pin: GPIO18
  mosi_pin: GPIO23
  miso_pin: GPIO19

# Configuration du serveur de fichiers web Box3Web
external_components:
  - source: github://your_username/esphome-components
    components: [box3web, samba]

box3web:
  url_prefix: "files"
  root_path: "/sdcard"
  sd_mmc_card: my_sd_card
  deletion_enabled: true
  download_enabled: true
  upload_enabled: true

# Configuration du serveur Samba
samba:
  share_name: "ESP32-Share"
  root_path: "/sdcard"
  sd_mmc_card: my_sd_card
