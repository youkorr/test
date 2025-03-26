# esphome_sd_card

SD MMC cards components for esphome.

## Config

```yaml
sd_mmc_card:
  id: sd_mmc_card
  mode_1bit: false
  clk_pin: GPIO14
  cmd_pin: GPIO15
  data0_pin: GPIO2
  data1_pin: GPIO4
  data2_pin: GPIO12
  data3_pin: GPIO13
  power_ctrl_pin: GPIO43  # Optionnel : GPIO pour contrôler l'alimentation de la carte SD
```

* **mode_1bit** (Optional, bool): spécifie si le mode 1 bit ou 4 bits est utilisé
* **clk_pin** : (Required, GPIO): broche d'horloge
* **cmd_pin** : (Required, GPIO): broche de commande
* **data0_pin**: (Required, GPIO): broche de données 0
* **data1_pin**: (Optional, GPIO): broche de données 1, utilisée uniquement en mode 4 bits
* **data2_pin**: (Optional, GPIO): broche de données 2, utilisée uniquement en mode 4 bits
* **data3_pin**: (Optional, GPIO): broche de données 3, utilisée uniquement en mode 4 bits
* **power_ctrl_pin**: (Optional, GPIO): broche pour contrôler l'alimentation de la carte SD (par exemple, GPIO43 pour l'ESP32-S3-Box-3)

### Contrôle d'alimentation (PWR_CTRL)

Pour les appareils comme l'ESP32-S3-Box-3, vous pouvez utiliser la broche `power_ctrl_pin` pour activer ou désactiver l'alimentation de la carte SD. Par exemple, sur l'ESP32-S3-Box-3, la broche GPIO43 est souvent utilisée pour contrôler l'alimentation du lecteur de carte SD.

Exemple de configuration pour l'ESP32-S3-Box-3 :
```yaml
sd_mmc_card:
  clk_pin: GPIO14
  cmd_pin: GPIO15
  data0_pin: GPIO2
  power_ctrl_pin: GPIO43  # Active l'alimentation du lecteur de carte SD
``` yaml

ftp_server 

ftp_server:
  username: ""  # Choisissez votre nom d'utilisateur
  password: ""  # Choisissez un mot de passe sécurisé
  root_path: "/"  # Chemin vers votre carte SD
  port: 21 


  box3web:
  id: box3_web
  url_prefix: files                    
  root_path: "/"                 
  enable_download: true                 
  enable_upload: false 

```
  


