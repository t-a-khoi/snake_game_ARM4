/*
 * game_display.h
 *
 *  Created on: Oct 21, 2025
 *      Author: DELL
 */

#ifndef INC_GAME_DISPLAY_H_
#define INC_GAME_DISPLAY_H_

#include "software_timer.h"
#include "lcd.h"
#include "touch.h"
#include "game_control.h"

#define SCREEN_X 10
#define SCREEN_Y 40
#define SCREEN_SIZE 220
#define CELL_SIZE 10
#define GRID_ROWS SCREEN_SIZE/CELL_SIZE
#define GRID_COLS SCREEN_SIZE/CELL_SIZE

extern uint8_t gameGrid[GRID_ROWS][GRID_COLS];

struct Snake {
    uint16_t headX, headY;
    uint16_t tailX, tailY;
    uint16_t color;
};

extern struct Snake snake;
enum Direction {
    UP, DOWN, LEFT, RIGHT
};

void displayPauseScreen(void);
extern enum Direction snakeDirection;

void renderScreen();

void generateFruit();

void initializeGame();

uint8_t moveSnake();

void advanceSnakeHead();

void advanceSnakeHeadTo(int16_t nx, int16_t ny);

void removeSnakeTail();

void handleInput();

void displayStartScreen(void);

void displayRetryButton(void);


#endif /* INC_GAME_DISPLAY_H_ */
