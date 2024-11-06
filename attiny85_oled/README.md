# WPT OLED in ATtiny85 board

![image(1)](https://github.com/user-attachments/assets/8dbc6fd8-4229-41bc-b4e8-03ac980868bc)


OLED display supplied by Wireless Power Transfer (WPT)


### Display protocol
I2C communication is ideal for OLED displays because of its low power consumption and reduced clock speed, which are crucial factors in applications like wireless power transfer (WPT) where energy efficiency is key. Using I2C with an SSD1306 OLED display minimizes power use and maintains smooth functionality without requiring high-speed communication, making it a practical choice for energy-sensitive designs like WPT systems.

### Libraries
Tiny4kOLED library is used. Other libraries such as u8g2, Adafruit_SSD1306 are too big to compile in the small size ATtiny85 flash memory (8k).
The Tiny4kOLED library supports GDDRAM (Graphic Display Data RAM) approach that is proposed from SSD1306 library, treating each block of 8 rows as a single "page" and reading pixel data in a bottom-to-top sequence. This data is then represented in hexadecimal format. For example, with a 64x32 pixel OLED display, the memory would store a total of 256 hexadecimal values (64 columns x 4 pages), each corresponding to an 8-row segment of the display.

![image](https://github.com/user-attachments/assets/2cec513f-f403-4856-9d65-a573549c177c)

It is possible to convert from dot pixel values in the image to byte array as mentioned above in this website :
https://javl.github.io/image2cpp 



### Reference code
https://github.com/upiir/arduino_oled_battery_indicator
I utilized the code from upiir and converted it to work with the Tiny4kOLED library, as the ATtiny85 lacks sufficient memory to support the u8g2 library.

### Difficulties 
It was not easy either to use the code in Tiny4kOLED library due to the followings :
1. ***Memory Constraints***: Images must be stored in flash memory to use the **bitmap()** function. With 30 images at 32x40 pixels, the total size approaches 4k. Additionally, the battery gauge image occupies 0.6k, and the remaining Tiny4kOLED code already consumes around 3.5k. Consequently, the total memory usage exceeds the 8k limit, preventing successful uploads.
2. ***Protocol Complexity***: The Tiny4kOLED library employs the original SSD1306 protocol, which complicates tasks such as shifting images by a few pixels or repeating them - 
Explanation : 
https://www.electronicwings.com/sensors-modules/ssd1306-oled-display
https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf (SSD1306 Datasheet)
3. ***Limitations of the **bitmap()** Function***: While the **bitmap()** function in Tiny4kOLED is powerful, it lacks the flexibility to repeat or crop images. The specified dimensions must match the image buffer size exactly, necessitating customization of the function to allow for cropping and repetition.
4. ***Displaying speed***: The ATtiny microcontroller has limited processing capabilities, which results in slower image rendering via I2C on the OLED display. Consequently, when drawing images repeatedly or clearing the screen, the animation may suffer from flickering or reduced performance. Ideally, the entire canvas would be utilized, with updates confined to the regions of interest (ROI). However, due to memory constraints, it is not feasible to accommodate the entire image.

![image-1](https://github.com/user-attachments/assets/bf0190c8-ee2b-462d-8973-ddb08a502c6d)

(As depicted in the ELF file, 5440 bytes are allocated solely for images!)
ELF command : <pre>readelf --all <filename.elf></pre>

While the original blob image is sized at 32x34 pixels, the Tiny4kOLED requires the height to be a multiple of 8, leading to an expansion to 40 rows and an increase in file size.

5. ***Power Consumption***: Since this project is to operate OLED with WPT system,  power consumption is always a constraint.


#### Proposed Solutions
1. To reduce the memory footprint below 8k, I focused on minimizing image size rather than reducing the overall image dimensions or the number of frames (30). I aimed to optimize the battery image by eliminating unnecessary pixels, such as those in the repetitive patterns of the battery gauge edge.
2. Useful website (https://javl.github.io/image2cpp/) can convert bitmap image into the format of SSD1306.
3. Customized **bitmap** function is used to crop or repeat the image.
4. By customizing **bitmap** function, instead of displaying the original bitmap function for repeated pattern under the loop function, sending the whole data repeatedly and then displaying helped a lot to reduce the speed.

<br>

![image-2](https://github.com/user-attachments/assets/c470ecb4-bc65-4187-9bfe-1bf5b9c62dab)

For example, to display battery gauge as the above, the whole rectangle area in sky blue color must be written in flash memory to display using **bitmap** function.

![image-3](https://github.com/user-attachments/assets/8df2aea3-5957-43cc-809f-548f910661bf)

To minimize unnecessary memory use, it’s more efficient to save only the essential image sections, repeating only the pattern within the red rectangle as needed.

<br>
To recreate this pattern, using a loop outside of the bitmap function is not ideal. This is because the bitmap function’s **startData()**, **sendData()**, and **endData()** commands consume time each time they are initialized, transmitted, and closed. Since **sendData()** handles one byte of address at a time (e.g., 0x0f), it’s possible to send a sequence of bytes in a single call (e.g., 0x0f, 0xff, 0xf0) before reaching **endData()**. Although it cannot send data across multiple pages (y-pixels in SSD1306), sending chunks of bytes together can help reduce transmission time.


Example) If I want to send this data 3 times 

<pre>
    {0x0f, 
    0xff,   
    0xf0} 
</pre>

The ideal case is to copy the data 3 times as it is :

<pre>
    {0x0f, 0x0f, 0x0f,
    0xff, 0xff, 0xff,
    0xf0, 0xf0, 0xf0}
</pre>

However, this approach isn’t feasible, because the data must be stored in flash memory (PROGMEM) in order to read address in **bitmap()**.


![image-4](https://github.com/user-attachments/assets/c0d63ce9-fff1-4b23-9164-9bce9d48cb54)


One option is to call the **bitmap()** function three times, but this results in **startData()**, **sendData()**, and **endData()** being called separately each time, increasing overhead.

A more efficient solution is to access and read the memory contents 3 times within a single **sendData()** function call. This way, **startData()**, **sendData()**, and **endData()** are each called only once.

Given that the battery edge shape is at least 50 pixels, this customization saves substantial time by minimizing repeated function calls.


5. To reduce power consumption, some special setups are applied in HW and SW.
- Software: Lower operating clock for ATtiny85 and OLED is used. Brightness is set as very dimmed condition. 

| ![https://github.com/user-attachments/assets/95217136-2894-43af-8f2d-bc97002f0a04](https://github.com/user-attachments/assets/95217136-2894-43af-8f2d-bc97002f0a04) | 
|:--:| 
| *Brightness setting in SSD1306 Datasheet (p.36)* |

```
oled.setContrast(20);
```

| ![https://github.com/user-attachments/assets/0e007b3d-ebde-4610-ab7a-a71653b9e519](https://github.com/user-attachments/assets/0e007b3d-ebde-4610-ab7a-a71653b9e519) | 
|:--:| 
| *Clock setting in SSD1306 Datasheet (p.22)* |

```
oled.setDisplayClock(2, 1);
```
First param is devide ratio and second param is oscillator frequency. In this code, it's set to 1/2 of default clock.

- Hardware : 
