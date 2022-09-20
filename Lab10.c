// Lab10.c
// Runs on TM4C123
// Jonathan Valvano and Daniel Valvano
// This is a starter project for the EE319K Lab 10

// Last Modified: 1/16/2021 
// http://www.spaceinvaders.de/
// sounds at http://www.classicgaming.cc/classics/spaceinvaders/sounds.php
// http://www.classicgaming.cc/classics/spaceinvaders/playguide.php
/* 
 Copyright 2021 by Jonathan W. Valvano, valvano@mail.utexas.edu
    You may use, edit, run or distribute this file
    as long as the above copyright notice remains
 THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
 OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
 VALVANO SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
 OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 For more information about my classes, my research, and my books, see
 http://users.ece.utexas.edu/~valvano/
 */
// ******* Possible Hardware I/O connections*******************
// Slide pot pin 1 connected to ground
// Slide pot pin 2 connected to PD2/AIN5
// Slide pot pin 3 connected to +3.3V 
// fire button connected to PE0
// special weapon fire button connected to PE1
// 8*R resistor DAC bit 0 on PB0 (least significant bit)
// 4*R resistor DAC bit 1 on PB1
// 2*R resistor DAC bit 2 on PB2
// 1*R resistor DAC bit 3 on PB3 (most significant bit)
// LED on PB4
// LED on PB5

// VCC   3.3V power to OLED
// GND   ground
// SCL   PD0 I2C clock (add 1.5k resistor from SCL to 3.3V)
// SDA   PD1 I2C data

//************WARNING***********
// The LaunchPad has PB7 connected to PD1, PB6 connected to PD0
// Option 1) do not use PB7 and PB6
// Option 2) remove 0-ohm resistors R9 R10 on LaunchPad
//******************************

#include <stdint.h>
#include "../inc/tm4c123gh6pm.h"
#include "../inc/CortexM.h"
#include "SSD1306.h"
#include "Print.h"
#include "Random.h"
#include "ADC.h"
#include "DAC.h"
#include "Images.h"
#include "Sound.h"
#include "Timer0.h"
#include "Timer1.h"
#include "TExaS.h"
#include "Switch.h"
//********************************************************************************
// debuging profile, pick up to 7 unused bits and send to Logic Analyzer
#define PB54                  (*((volatile uint32_t *)0x400050C0)) // bits 5-4
#define PF321                 (*((volatile uint32_t *)0x40025038)) // bits 3-1
// use for debugging profile
#define PF1       (*((volatile uint32_t *)0x40025008))
#define PF2       (*((volatile uint32_t *)0x40025010))
#define PF3       (*((volatile uint32_t *)0x40025020))
#define PB5       (*((volatile uint32_t *)0x40005080)) 
#define PB4       (*((volatile uint32_t *)0x40005040)) 
// TExaSdisplay logic analyzer shows 7 bits 0,PB5,PB4,PF3,PF2,PF1,0 
// edit this to output which pins you use for profiling
// you can output up to 7 pins
void LogicAnalyzerTask(void){
  UART0_DR_R = 0x80|PF321|PB54; // sends at 10kHz
}
void ScopeTask(void){  // called 10k/sec
  UART0_DR_R = (ADC1_SSFIFO3_R>>4); // send ADC to TExaSdisplay
}
// edit this to initialize which pins you use for profiling
void Profile_Init(void){
  SYSCTL_RCGCGPIO_R |= 0x22;      // activate port B,F
  while((SYSCTL_PRGPIO_R&0x20) != 0x20){};
  GPIO_PORTF_DIR_R |=  0x0E;   // output on PF3,2,1 
  GPIO_PORTF_DEN_R |=  0x0E;   // enable digital I/O on PF3,2,1
  GPIO_PORTB_DIR_R |=  0x30;   // output on PB4 PB5
  GPIO_PORTB_DEN_R |=  0x30;   // enable on PB4 PB5  
}
//********************************************************************************

void PortF_Init(void)
{
	SYSCTL_RCGCGPIO_R |= 0x20;
	volatile int delay = SYSCTL_RCGCGPIO_R;
	GPIO_PORTF_LOCK_R = 0x4C4F434B;
	GPIO_PORTF_CR_R = 0x1F;
	GPIO_PORTF_DEN_R |= 0x1F;
	GPIO_PORTF_DIR_R &= 0x0E;  //xxx0 1110
	GPIO_PORTF_PUR_R |= 0x11;
	
	GPIO_PORTF_IS_R &= ~0x10;
	GPIO_PORTF_IBE_R &= ~0x10;  //not both edges
	GPIO_PORTF_IEV_R |= 0x01;  //rising edge
	GPIO_PORTF_ICR_R = 0x10;
	GPIO_PORTF_IM_R |= 0x10;
	NVIC_PRI7_R = (NVIC_PRI7_R & 0xFF00FFFF) | 0x00A00000;  //bits 23-21: priority for port F
	NVIC_EN0_R = 0x40000000;
}

void SysTick_Init(uint32_t period)
{
	NVIC_ST_CTRL_R = 0;
	NVIC_ST_RELOAD_R = period - 1;
	NVIC_ST_CURRENT_R = 0;
	NVIC_ST_CTRL_R = 0x00000007;
}
 
void Delay100ms(uint32_t count); // time delay in 0.1 seconds

uint8_t paused = 0;  //0 = not paused, 1 = paused
void Pause(void)
{
	//GPIO_PORTF_DATA_R ^= 0x03;  //heartbeat
	static uint8_t continuousPress = 0;
	if (((GPIO_PORTF_DATA_R & 0x01) == 0) && (continuousPress == 0))
	{
		paused ^= 0x01;
		continuousPress = 1;
	}
	else if ((GPIO_PORTF_DATA_R & 0x01) == 1)
		continuousPress = 0;
}

void (*PausePtr)(void) = &Pause;

typedef enum {dead, alive} status_t;
struct sprite
{
	int32_t x;
	int32_t y;
	const uint8_t *image;
	status_t life;
};
typedef struct sprite sprite_t;

sprite_t player = {56, 64, PlayerShip0, alive};
sprite_t Enemy[18];
sprite_t Missile[18];

uint8_t NeedToDraw = 0;
uint8_t language;  //0 = english, 1 = spanish
uint32_t score = 0;

void Draw(void);
void Move(void);
void CheckForCollisions(void);
void FireMissile(void);
void SpawnEnemies(void);

char WinMessageEng[] = "You saved the planet!";
char WinMessageSpa[] = "\xB2Salvaste el planeta!";
char LoseMessageEng[] = "Game over!";
char LoseMessageSpa[] = "\xB2""Fin del juego!";

char ScoreEng[] = "Score: ";
char ScoreSpa[] = "Marcador: ";

char *WinMessageEngPtr = WinMessageEng;
char *WinMessageSpaPtr = WinMessageSpa;
char *LoseMessageEngPtr = LoseMessageEng;
char *LoseMessageSpaPtr = LoseMessageSpa;

char *ScoreEngPtr = ScoreEng;
char *ScoreSpaPtr = ScoreSpa;

int mainstarter(void){uint32_t time=0;
  DisableInterrupts();
  // pick one of the following three lines, all three set to 80 MHz
  //PLL_Init();                   // 1) call to have no TExaS debugging
  TExaS_Init(&LogicAnalyzerTask); // 2) call to activate logic analyzer
  //TExaS_Init(&ScopeTask);       // or 3) call to activate analog scope PD2
  SSD1306_Init(SSD1306_SWITCHCAPVCC);
  SSD1306_OutClear();   
  Random_Init(1);
  Profile_Init(); // PB5,PB4,PF3,PF2,PF1 
  SSD1306_ClearBuffer();
  SSD1306_DrawBMP(2, 62, SpaceInvadersMarquee, 0, SSD1306_WHITE);
  SSD1306_OutBuffer();
  EnableInterrupts();
  Delay100ms(20);
  SSD1306_ClearBuffer();
  SSD1306_DrawBMP(47, 63, PlayerShip0, 0, SSD1306_WHITE); // player ship bottom
  SSD1306_DrawBMP(53, 55, Bunker0, 0, SSD1306_WHITE);

  SSD1306_DrawBMP(0, 9, Alien10pointA, 0, SSD1306_WHITE);
  SSD1306_DrawBMP(20,9, Alien10pointB, 0, SSD1306_WHITE);
  SSD1306_DrawBMP(40, 9, Alien20pointA, 0, SSD1306_WHITE);
  SSD1306_DrawBMP(60, 9, Alien20pointB, 0, SSD1306_WHITE);
  SSD1306_DrawBMP(80, 9, Alien30pointA, 0, SSD1306_WHITE);
  SSD1306_DrawBMP(50, 19, AlienBossA, 0, SSD1306_WHITE);
  SSD1306_OutBuffer();
  Delay100ms(30);

  SSD1306_OutClear();  
  SSD1306_SetCursor(1, 1);
  SSD1306_OutString("GAME OVER");
  SSD1306_SetCursor(1, 2);
  SSD1306_OutString("Nice try,");
  SSD1306_SetCursor(1, 3);
  SSD1306_OutString("Earthling!");
  SSD1306_SetCursor(2, 4);
  while(1){
    Delay100ms(10);
    SSD1306_SetCursor(19,0);
    SSD1306_OutUDec2(time);
    time++;
    PF1 ^= 0x02;
  }
}

int main(void) {
	DisableInterrupts();
	PortF_Init();
	DAC_Init();
	Sound_Init();
	ADC_Init();
	// pick one of the following three lines, all three set to 80 MHz
  //PLL_Init();                   // 1) call to have no TExaS debugging
  TExaS_Init(&LogicAnalyzerTask); // 2) call to activate logic analyzer
  //TExaS_Init(&ScopeTask);       // or 3) call to activate analog scope PD2
  SSD1306_Init(SSD1306_SWITCHCAPVCC);
  SSD1306_OutClear();   
  Random_Init(3);
	SysTick_Init(4000000);  //20 Hz
	
	//enemy initialization
	for(int i = 0; i < 3; i++)
	{
		Enemy[i].x = 30*(i+1);
		Enemy[i].y = 10;
		Enemy[i].image = Invader;
		Enemy[i].life = alive;
	}
	for(int i = 3; i < 18; i++)
	{
		Enemy[i].image = Invader;
		Enemy[i].life = dead;
	}

	//missile initialization
	for(int i = 0; i < 18; i++)
	{
		Missile[i].image = Missile0;
		Missile[i].life = dead;
	}
	
	//start screen
	SSD1306_DrawBMP(0, 63, SpaceInvadersMarquee, 0, SSD1306_INVERSE);
	SSD1306_OutBuffer();
	
	uint8_t SW2data = 1;
	uint8_t SW1data = 1;
	
	while ((SW2data == 1) && (SW1data == 1))			//stays on start screen until button press
	{  
		//wait for button to be pressed
		SW2data = GPIO_PORTF_DATA_R & 0x01;
		SW1data = (GPIO_PORTF_DATA_R & 0x10) >> 4;
	}
	
	//language select screen
	SSD1306_OutClear(); 
	SSD1306_ClearBuffer();
	SSD1306_SetCursor(0, 1);
	SSD1306_OutString("SW1: English");
	SSD1306_SetCursor(0, 2);
	SSD1306_OutString("SW2: Espanol");		//"SW2: Espa\xA4""ol"
	
	while (((GPIO_PORTF_DATA_R & 0x10) == 0x00) || ((GPIO_PORTF_DATA_R & 0x01) == 0x00)) {};  //wait for both switches to be released
		
	SW2data = 1;
	SW1data = 1;
	while ((SW2data == 1) && (SW1data == 1))
	{  
		//wait for button to be pressed
		SW2data = GPIO_PORTF_DATA_R & 0x01;
		SW1data = (GPIO_PORTF_DATA_R & 0x10) >> 4;
	}
	if ((GPIO_PORTF_DATA_R & 0x10) == 0x00) //SW1 pressed
		language = 0;  //english
	else
		language = 1;  //spanish
	
	//game start
	EnableInterrupts();
	Timer0_Init(PausePtr, 8000);
	
	//game engine loop
	while(player.life)
	{
		while (paused == 1) 
		{
			NVIC_ST_CTRL_R = 0;
		}
		NVIC_ST_CTRL_R = 0x00000007;
		if (NeedToDraw)
			Draw();
		if (score >= 24)
			player.life = dead;
	}
	
	//player dies
	SSD1306_OutClear(); 
	SSD1306_ClearBuffer();
	
	SSD1306_SetCursor(0,1);
	if (language == 0) //english
	{
		if (score >= 24)
			SSD1306_OutString(WinMessageEngPtr);
		else
			SSD1306_OutString(LoseMessageEngPtr);
		
		SSD1306_SetCursor(0,2);
		SSD1306_OutString(ScoreEngPtr);
		LCD_OutDec(score);
	}
	else  //spanish
	{
		if (score >= 24)
			SSD1306_OutString(WinMessageSpaPtr);
		else
			SSD1306_OutString(LoseMessageSpaPtr);
		
		SSD1306_SetCursor(0,2);
		SSD1306_OutString(ScoreSpaPtr);
		LCD_OutDec(score);
	}
		
	
}

// You can't use this timer, it is here for starter code only 
// you must use interrupts to perform delays
void Delay100ms(uint32_t count){uint32_t volatile time;
  while(count>0){
    time = 727240;  // 0.1sec at 80 MHz
    while(time){
	  	time--;
    }
    count--;
  }
}

void Draw(void)
{
	SSD1306_ClearBuffer();
	//enemies and missiles
	for(int i = 0; i < 18; i++)
	{
		if (Enemy[i].life)
			SSD1306_DrawBMP(Enemy[i].x, Enemy[i].y, Enemy[i].image, 0, SSD1306_INVERSE);
		if (Missile[i].life)
			SSD1306_DrawBMP(Missile[i].x, Missile[i].y, Missile[i].image, 0, SSD1306_INVERSE);
	}
	//player
	if (player.life)
		SSD1306_DrawBMP(player.x, player.y, player.image, 0, SSD1306_INVERSE);
	
	SSD1306_OutBuffer();
	NeedToDraw = 0;
}

void Move(void)
{
	for (int i = 0; i < 18; i++)
	{
		if(Enemy[i].life == alive)
		{
			NeedToDraw = 1;
			if(Enemy[i].y < 62)
				Enemy[i].y += 1;  //move down
			else
				player.life = dead;
		}
		if(Missile[i].life == alive)
		{
			NeedToDraw = 1;
			if(Missile[i].y > 2)
				Missile[i].y -= 6; //move up
			else
				Missile[i].life = dead; //off-screen
		}
	}
	//move player
	if(player.life == alive)
	{
		NeedToDraw = 1;
		uint32_t ADCdata = ADC_In();
		player.x = ((111)*ADCdata)/4096;  //111 = 127-16
	} 
}

void CheckForCollisions(void)
{
	uint32_t x1, y1, x2, y2;
	
	for (int i = 0; i < 18; i++)
	{
		if (Enemy[i].life == alive)
		{
			x1 = Enemy[i].x + 5;
			y1 = Enemy[i].y - 4;
			for (int j = 0; j < 18; j++)
			{
				if (Missile[j].life == alive)
				{
					x2 = Missile[j].x + 1;
					y2 = Missile[j].y - 4;
					
					//check for collsion
					if(((x1-x2)*(x1-x2)+(y1-y2)*(y1-y2)) < 100)
					{
						//collision detected
						Enemy[i].life = dead;
						Missile[j].life = dead;
						score++;
					}
				}
			}
		}
	}
}

void FireMissile(void)
{
	//GPIO_PORTF_DATA_R ^= 0x03;  //heartbeat
	int i = 0;
	while (i < 18)
	{
		if (Missile[i].life == dead)
		{
			Missile[i].x = player.x + 8;
			Missile[i].y = player.y - 6;
			Missile[i].life = alive;
			return;
		}
		i++;
	}
	return;
}

void SpawnEnemies(void)
{
	uint32_t enemyCount = (Random32() >> 16) % 3;  // 0 to 3
	uint8_t enemiesSpawned = 0;
	for (int i = 0; i < 18; i++)
	{
		if (enemiesSpawned >= enemyCount)
			return;
		if (Enemy[i].life == dead)
		{
			Enemy[i].x = ((Random32() >> 16) % 100) + 8;			//((Random32() >> 16) % 100) + 15;
			Enemy[i].y = 10;
			Enemy[i].life = alive;
			enemiesSpawned++;
		}
	}
	return;
}

void SysTick_Handler(void)
{
	uint8_t now = (GPIO_PORTF_DATA_R & 0x10) >> 4;
	static uint8_t last;
	if ((now == 0) && (last == 1))
	{
		FireMissile();
		Sound_Start();
	}
	
	//GPIO_PORTF_DATA_R ^= 0x03;  //heartbeat
	
	if (Random() <= 25)
		SpawnEnemies();
	Move();
	CheckForCollisions();
	
	last = now;
}

void GPIOPortF_Handler(void)
{
	GPIO_PORTF_ICR_R = 0x10;
	//GPIO_PORTF_DATA_R ^= 0x03;  //heartbeat
}
