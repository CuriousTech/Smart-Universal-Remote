# Smart-Universal-Remote  
Smart Universal Remote using one of these:  
[Waveshare ESP32-S3-Touch-LCD-1.28](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.28)  
[Waveshare ESP32-S3-Touch-LCD-1.69](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.69)  
[WaveShare-ESP32-S3-Touch-LCD-2.8](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-2.8)  
[WaveShare-ESP32-S3-Touch-LCD-2inch](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-2)  
  
![uremote](https://curioustech.net/images/uremote.jpg)  
![uremote2](https://curioustech.net/images/ur.gif)  
  
This is a tiny smart universal remote with WiFi, IR, capacitive touchscreen, wireless charging (and possibly Bluetooth) all for a very low cost.  
  
Parts list:  
  
[AliExpress ESP32-S3-Touch-1.28](https://www.aliexpress.us/item/3256806026101753.html?spm=a2g0o.order_list.order_list_main.5.eb321802K7vxRh&gatewayAdapt=glo2usa) This includes the 12p connector with dupont ends.  
  
[AliExpress ESP32-S3-Touch-1.69](https://www.aliexpress.us/item/3256806781994387.html?spm=a2g0o.order_list.order_list_main.5.6e661802Im2eg3&gatewayAdapt=glo2usa)  Or this one. It seems they've fixed the original issue, but I'm not buying another one to test it. [Subreddit thread](https://www.reddit.com/r/esp32/comments/1cxmo5r/esp32s3_169inch_touch_display_features_6axis_imu/)  
  
[AliExpress ESP32-S3-Touch-2.8](https://www.aliexpress.us/item/3256807168316377.html?spm=a2g0o.order_list.order_list_main.11.654e1802WfmOyY&gatewayAdapt=glo2usa) My new favorite. Comes with dupont connectors, speakers (not yet implemented). No M2.5 screws, but not hard to find. The wireless charging pad may need to go on the back of the case due to screws.  
  
[eBay:Charger pad](https://www.ebay.com/itm/143351559508?var=442544081497) Select USB-C. These can be bought anywhere.  For the latest model, the charger can be centered. The edges of the pad can be bent because the wrapper is a bit oversized. A USB-C cable or magnetic connector will work as well.  

[eBay:battery](https://www.ebay.com/itm/174781170731?var=473957762104) Select 1.25mm plug. This is probably a decent size. Be careful with the polarity. It was correct on mine. They sell a 1500mAh that's just a little thicker, but I may add a keypad that will use the extra space.   

[TinkerCAD Case Models](https://www.tinkercad.com/things/j1XckJlfVuT-waveshare-esp32-s3-touch-128-remote-case) For editing the 3D print models.  
  
[New IRIO board on OSHPark](https://oshpark.com/shared_projects/KRJOFbjO)  I used some double-sided tape to make sure it doesn't touch the other board.  
  
This IR sender/receiver is designed to be built easily using common parts, but Arduino IR modules should be compatible.  If using an NPN transistor for the sender, subtract the vF from 3.3 for R1 plus the IR LED vF. 110 ohms should be good for a MOSFET and 1.2V LED.  
  
The web page. This is used to decode remotes and test output commands. Just hit the decode button and start pushing buttons on a remote. Those values can be entered in the button code[4] array of any buttons created. Mixed remotes on one page, or all the same. Doesn't matter.  
![uremoteweb](https://curioustech.net/images/uremoteweb.png)  
