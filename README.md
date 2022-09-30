esp_influx
==========

This repository contains code for running a small ESP32 LoRA network of DHT22 temperature and humidity sensors, which push data to a local InfluxDB installation.

This does not require a LoRA gateway or concentrator. One of the ESP32 LoRA modules acts as a receiver for all other "nodes".

Hardware
--------

This has been tested with LILIGO ESP32 LoRA (https://www.aliexpress.com/item/32872078587.html?spm=a2g0o.store_pc_groupList.8148356.15.5bd1391cdaYoBA&pdp_npi=2%40dis%21AUD%21AU%20%2426.56%21AU%20%2426.56%21%21%21%21%21%402100bdec16645014319921715e42bf%2112000022840374525%21sh) modules.

The DHT sensor is connected to IO13 (and the 3.3V and GND) ports on the ESP32.

A small box is recommended for waterproofing if using outside:

![Alt text](images/node_inside.jpg?raw=true "Inside a node box")
![Alt text](images/nodes.jpg?raw=true "All the node boxes")

Software
--------

The overall architecture is:
- LoRA Nodes poll attached DHT22 sensors for temperature and humidity readings
- LoRA Nodes send this information (plus module voltage and unique device ID) via (encrypted) LoRA to a Main Node
- The Main Node decrypts the readings, adds in the RSSI value and onsends this to a local InfluxDB installation via Wifi.

The Lora Nodes are configured for very low power operation. They will read the DHT22 sensors once per minute, going into a deep sleep mode in between readings. A typical healthy 18650 battery should give 2 years operation. See ``ESP32 Power.ods`` for the calculations.

The InfluxDB install can be on a Raspberry Pi or similar. The InfluxDB IP address, database name, username and password can be configured in ``TTGO_LoraMain``. Grafana is recommended for data visualisation:

![Alt text](images/grafana1.png?raw=true "Grafana View 1")
![Alt text](images/grafana2.png?raw=true "Grafana View 2")
