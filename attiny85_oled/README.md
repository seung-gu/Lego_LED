# WPT OLED in ATtiny85 board

![Alt text](image.png)

OLED display supplied by Wireless Power Transfer (WPT)

### Choosing Inductor
Receiving coil is the most important part for WPT. It must be always resonance circuit for the best effectiveness of WPT.
Fequency of transmission coil is 142kHz, therefore, there are a lot of combination depending on the inductance and capacitance.
![image](https://github.com/user-attachments/assets/3431a45d-f5e7-4bc9-bd39-d53788a87f1e)


These are all combinations between the inductance 100uH ~ 1.5mH from the website lcsc.com.

![image](https://github.com/user-attachments/assets/c208222e-8f89-4b37-97d2-d3445d933d10)

With resonance, another important factor is DCR in inductor. 
Normally, if the inductance is higher, the resistance in the inductor (DCR) is higher.

![image](https://github.com/user-attachments/assets/5d1ca951-8a10-4083-8f6c-0666aab376e0)

Therefore, the first image shows the relation between the factor and its inductance.
Bubble size is the size of inductor, which is the standard size as it's shown -

![image](https://github.com/user-attachments/assets/b5966daf-c049-48d6-a9a7-88806e39ad15)

Four inductors (470uH, 680uH, 820uH, 1500uH) are chosen according to the highest factor values.
