#ifndef INC_GAME_CONTROL_H_
#define INC_GAME_CONTROL_H_

#include "software_timer.h"
#include "lcd.h"
#include "touch.h"
#include "game_display.h"
#include "button.h"


#define DIRECTION_BTN_X 80
#define DIRECTION_BTN_Y 240
#define DIRECTION_BTN_SIZE 20
extern uint8_t selectedMap;
void gameFSM(void);

uint8_t isStartScreenTouched(void);

uint8_t isRetryButtonTouched(void);

void updateSnakeDirection(void);

uint8_t isButtonUp(void);

uint8_t isButtonDown(void);

uint8_t isButtonLeft(void);

uint8_t isButtonRight(void);
uint8_t isPhyButtonUpEdge(void);  //new
uint8_t isPhyButtonDownEdge(void);
uint8_t isPhyButtonLeftEdge(void);
uint8_t isPhyButtonRightEdge(void);
uint8_t isPhyStartEdge(void);
uint8_t isPhyPauseEdge(void);


uint8_t isHomeButtonTouched(void);
uint8_t isPauseButtonTouched(void);

#endif /* INC_GAME_CONTROL_H_ */
