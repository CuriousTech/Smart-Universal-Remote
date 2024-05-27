# Smart-Universal-Remote
Smart Universal Remote using [Waveshare ESP32-S3-Touch-LCD-1.28](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.28) or [Waveshare ESP32-S3-Touch-LCD-1.69](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.69)    
  
This is a work in progress and not all code is complete. That said, it functionally works.  
  
![uremote1](https://curioustech.net/images/uremote1.jpg)  
![uremote2](https://curioustech.net/images/uremote2.jpg)  
![uremote2](https://curioustech.net/images/wave.gif)  
  
This is a tiny smart universal remote with WiFi, IR, capacitive touchscreen, wireless charging (and possibly Bluetooth) all for a very low cost.  
  
Parts list:  
  
[AliExpress ESP32-S3-Touch-1.28](https://www.aliexpress.us/item/3256806026101753.html?spm=a2g0o.order_list.order_list_main.5.eb321802K7vxRh&gatewayAdapt=glo2usa) This includes the 12p connector with dupont ends.  
  
[AliExpress ESP32-S3-Touch-1.69](https://www.aliexpress.us/item/3256806781994387.html?spm=a2g0o.order_list.order_list_main.5.6e661802Im2eg3&gatewayAdapt=glo2usa)  Or this one. Warning, though. The PSRAM on mine is nonfunctioning, and WiFi is sproratic at best, so I won't be using it. They both have the same main components, so it's probably just defective.  
  
[eBay:Charger pad](https://www.ebay.com/itm/143351559508?var=442544081497) Select USB-C. These can be bought anywhere.  As shown, place it face down, which flips the connector so the wire/ribbon is on top. The edges of the pad can be bent because the wrapper is a bit oversized.  

[eBay:battery](https://www.ebay.com/itm/174781170731?var=473957762104) Select 1.25mm plug. This is probably a decent size. Be careful with the polarity. It was correct on mine. They sell a 1500mAh that's just a little thicker, but I may add a keypad that will use the extra space.   

[TinkerCAD Case Model](https://www.tinkercad.com/things/j1XckJlfVuT-waveshare-esp32-s3-touch-128-remote-case) For editing the 3D print model.  The 1.69" model is here as well.  

[Old IRIO board on OSHPark](https://oshpark.com/shared_projects/fLeru7yH)  Solder a 4 pin header flat to it.  

This IR sender/receiver is designed to be built easily using common parts, but Arduino IR modules should be compatible.  The top-side components are for the IR. If using an NPN transistor for the sender, subtract the vF from 3.3 for R1 plus the IR LED vF. 110 ohms should be good for a MOSFET and 1.2V LED. The bottom side is for battery voltage reading, and is unused for this.  

The web page. This is used to decode remotes and test output commands. Just hit the decode button and start pushing buttons on a remote. Those values can be entered in the button code[4] array of any buttons created. Mixed remotes on one page, or all the same. Doesn't matter.  
![uremoteweb](https://curioustech.net/images/uremoteweb.png)  
