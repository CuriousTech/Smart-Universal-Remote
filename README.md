# Smart-Universal-Remote
Smart Universal Remote using ESP32-2424S012  
  
Note: this project will soon be using the [Waveshare ESP32-S3-LCD-1.28](www.waveshare.com/wiki/ESP32-S3-LCD-1.28)  

This is a work in progress and not all code is complete. That said, it functionally works.  
  
![uremote](https://curioustech.net/images/uremote.png)  
  
This is a tiny smart universal remote with WiFi, IR, capacitive touchscreen, wireless charging (and possibly Bluetooth) all for a very low cost.  
  
Parts list:  

[ESP32-2424S012](https://www.aliexpress.us/item/3256805375174366.html?spm=a2g0o.order_list.order_list_main.5.1a5a1802KTy5Kg&gatewayAdapt=glo2usa) Get the one with touch and case, which also adds a small antenna. Just push it out and be careful with the antenna which is stuck to the back of the case.  

[eBay:Charger pad](https://www.ebay.com/itm/143351559508?var=442544081497) Select USB-C. These can be bought anywhere.  As shown, place it face down, which flips the connector so the "wire" thing is on top. The edges of the pad can be bent because the wrapper is a bit oversized.  

[eBay:battery](https://www.ebay.com/itm/174781170731?var=473957762104) Select 1.25mm plug. This is probably a decent size. I ordered it a month ago, haven't received it yet. Be careful with the polarity. All cells that I already have were reversed from what this needs. Swap the pins if so.  

[IRIO board on OSHPark](https://oshpark.com/shared_projects/fLeru7yH)

SH1.0mm 4P connector. Look on ebay or other sites.  

[Documents and examples](http://pan.jczn1688.com/directlink/1/ESP32%20module/1.28inch_ESP32-2424S012.zip)  

It has several drawbacks, though. First, the Li-Ion charger [IP5306](http://www.injoinic.com/wwwroot/uploads/files/20200221/0405f23c247a34d3990ae100c8b20a27.pdf) is designed for a powerbank. It blacks out when the source power is removed, so it will restart when the remote is removed from the charger. It also cuts battery power when the load drops below 45mA for around 32 seconds, so using power saving features can be difficult. This code pulses the LED backlight for a short period when dim to trick it.  

The ESP32C3 has 4MB flash, and around 380KB SRAM, which really isn't enough for BLE, WiFi, and good graphics.  I'm using TFT_eSPI and one 240x240 sprite for smooth screen swipes, oversize page scrolling, and all around drawing without flicker, but 2 fullsize sprites would allow for better swipe effects. Adding BLE (the most memory efficient I could find) with just the 1 sprite runs well over the space available, unless the sprite is removed, and possibly disable OTA to double the flash size. Currently this is flashed as Minimal SPIFFS (1.9MB APP with OTA) and SPIFFS is unused.  

SERVER_ENABLE can be disabled as well to reduce space. It's currently just used to test IR remote codes, and will display received data as well.  

[TinkerCAD Case Model](https://www.tinkercad.com/things/gB9CoUS346H-esp32-2424s012-remote) For editing the 3D print model.  

The IR sender/receiver is designed to be built easily using common parts, but Arduino IR modules should be compatible.  The top-side components are for the IR. If using an NPN transistor for the sender, subtract the vF from 3.3 for R1 plus the IR LED vF. 110 ohms should be good for a MOSFET and 1.2V LED. The bottom side is for battery voltage reading, since there is no way to determine voltage or charging/discharging state. This will connect to the IO8 pad (which has ADC) and BAT+, and can be triggered by pulsing the IR TX output.  

The web page. This is used to decode remotes and test output commands. Just hit the decode button and start pushing buttons on a remote. Those values can be entered in the button code[4] array of any buttons created. Mixed remotes on one page, or all the same. Doesn't matter.  
![uremoteweb](https://curioustech.net/images/uremoteweb.png)  
