# WPT OLED in ATtiny85 board

![Alt text](image.png)

OLED display supplied by Wireless Power Transfer (WPT)


### Display protocol
I2C communication is ideal for OLED displays because of its low power consumption and reduced clock speed, which are crucial factors in applications like wireless power transfer (WPT) where energy efficiency is key. Using I2C with an SSD1306 OLED display minimizes power use and maintains smooth functionality without requiring high-speed communication, making it a practical choice for energy-sensitive designs like WPT systems.

### Libraries
Tiny4kOLED library is used. Other libraries such as u8g2, Adafruit_SSD1306 are too big to compile in the small size ATtiny85 flash memory (8k).
The Tiny4kOLED library uses a unique display approach, treating each block of 8 rows as a single "page" and reading pixel data in a bottom-to-top sequence. This data is then represented in hexadecimal format. For example, with a 64x32 pixel OLED display, the memory would store a total of 256 hexadecimal values (64 columns x 4 pages), each corresponding to an 8-row segment of the display.

![image](https://github.com/user-attachments/assets/2cec513f-f403-4856-9d65-a573549c177c)
