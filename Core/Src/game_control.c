/*
 * game_control.c
 *
 *  Created on: Oct 21, 2025
 *      Author: DELL
 */

#include "game_control.h"

#define BTN_IDX_UP      1 //new
#define BTN_IDX_DOWN    9
#define BTN_IDX_LEFT    4
#define BTN_IDX_RIGHT   6
uint8_t selectedMap = 0;

typedef enum {
    GAME_INIT, GAME_START, GAME_PLAY, GAME_OVER, GAME_COLOR_SELECT, GAME_PAUSE, GAME_MAP_SELECT
} GameState;

static GameState currentState = GAME_START;

typedef struct {
    uint16_t xStart, yStart, xEnd, yEnd;
    uint8_t isPressed;
} ControlButton;

static ControlButton controlButtons[4];
static uint16_t score = 0;
static uint8_t startScreenDrawn = 0;
static uint8_t gameUIRendered = 0;

/* ====== Score UI (giữ nguyên vị trí tương thích layout hiện có) ====== */
#define SCORE_LABEL_X  70
#define SCORE_LABEL_Y  10
#define SCORE_NUM_X    120
#define SCORE_NUM_Y    10
#define SCORE_NUM_W    48
#define SCORE_NUM_H    16
static int lastScore = -1;
// [NEW] Khóa chống dính khi vừa vào GAME_START
static uint8_t startInputLock = 0;


static void updateScoreUI(void) {
    if (!gameUIRendered) return;
    if (lastScore == score) return;

    lcd_Fill(SCORE_NUM_X, SCORE_NUM_Y,
             SCORE_NUM_X + SCORE_NUM_W,
             SCORE_NUM_Y + SCORE_NUM_H,
             WHITE);
    lcd_ShowIntNum(SCORE_NUM_X, SCORE_NUM_Y, score, 3, BLACK, WHITE, 16);
    lastScore = score;
}
/* ================================================================ */

/* ====== Game Over overlay (THÊM MỚI) ====== */
static uint8_t gameOverScreenDrawn = 0;

/* Khung và nút RESTART */
#define GO_BOX_X1    20
#define GO_BOX_Y1    70
#define GO_BOX_X2    220
#define GO_BOX_Y2    200

#define GO_BTN_W     100
#define GO_BTN_H     30
#define GO_BTN_X1    ((240 - GO_BTN_W)/2)
#define GO_BTN_Y1    (GO_BOX_Y2 - 40)
#define GO_BTN_X2    (GO_BTN_X1 + GO_BTN_W)
#define GO_BTN_Y2    (GO_BTN_Y1 + GO_BTN_H)

static void displayGameOverScreen(void) {
    /* [NEW] clear full screen to avoid overlay */
    lcd_Fill(0, 0, 240, 320, BLACK);

    /* hộp */
    lcd_DrawRectangle(GO_BOX_X1, GO_BOX_Y1, GO_BOX_X2, GO_BOX_Y2, WHITE);
    lcd_Fill(GO_BOX_X1+1, GO_BOX_Y1+1, GO_BOX_X2-1, GO_BOX_Y2-1, BLACK);

    /* tiêu đề + điểm */
    lcd_ShowStr(65, GO_BOX_Y1 + 15, "GAME OVER", RED, BLACK, 24, 1);
    lcd_ShowStr(70, GO_BOX_Y1 + 50, "Score:", WHITE, BLACK, 16, 0);
    lcd_ShowIntNum(120, GO_BOX_Y1 + 50, score, 3, WHITE, BLACK, 16);

    /* nút RESTART */
    lcd_Fill(GO_BTN_X1, GO_BTN_Y1, GO_BTN_X2, GO_BTN_Y2, WHITE);
    lcd_DrawRectangle(GO_BTN_X1, GO_BTN_Y1, GO_BTN_X2, GO_BTN_Y2, BLACK);
    lcd_ShowStr(GO_BTN_X1 + 15, GO_BTN_Y1 + 7, "RESTART", BLACK, WHITE, 16, 0);
}


static inline uint8_t isRestartTouched(void) {
    return touch_IsTouched() &&
           touch_GetX() > GO_BTN_X1 && touch_GetX() < GO_BTN_X2 &&
           touch_GetY() > GO_BTN_Y1 && touch_GetY() < GO_BTN_Y2;
}
/* ========================================== */

void gameFSM(void) {
    switch (currentState) {
        case GAME_INIT:
            score = 0;
            led7_show_score(0);               /* nếu không dùng LED7, bỏ dòng này */
            currentState = GAME_START;
            startInputLock = 1;   // [NEW] chờ nhả tay sau khi vào màn Start
            break;

        case GAME_START:
        	// [NEW] Nếu đang khóa: chỉ chờ nhả tay rồi mới cho bấm START/đổi màu
        	if (startInputLock) {
        	    if (!touch_IsTouched()) {
        	        startInputLock = 0;   // đã nhả → mở khóa
        	    }
        	    led7_show_score(0);       // giữ nguyên hiển thị nếu bạn muốn
        	    break;                    // thoát case, KHÔNG xử lý startScreenHandleColorTouch/isStartScreenTouched
        	}
            if (!startScreenDrawn) {
                displayStartScreen();
                startScreenDrawn = 1;
            }
            {
                uint16_t c = startScreenHandleColorTouch();
                if (c != 0) {
                    snake.color = c;
                    displayStartScreen();
                }
            }
            if (isStartScreenTouched()) {
                currentState = GAME_MAP_SELECT;
                lcd_Fill(0,0,240,320,BLACK);
                displayMapSelectScreen();
            }
            led7_show_score(0);
            break;

        case GAME_PLAY:
            if (!gameUIRendered) {
                initializeButtons();
                gameUIRendered = 1;
                updateScoreUI();
            }

            if (button_read_flag) {
                setTimer_button(5);
                handleInput();
            }

            if (snake_move_flag) {
                HAL_GPIO_TogglePin(DEBUG_LED_GPIO_Port, DEBUG_LED_Pin);

                int16_t nextX = (int16_t)snake.headX;
                int16_t nextY = (int16_t)snake.headY;

                switch (snakeDirection) {
                    case UP:    nextY--; break;
                    case DOWN:  nextY++; break;
                    case LEFT:  nextX--; break;
                    case RIGHT: nextX++; break;
                }

                /* wrap biên */
                if (nextX < 0)                nextX = GRID_ROWS - 1;
                else if (nextX >= GRID_ROWS)  nextX = 0;
                if (nextY < 0)                nextY = GRID_COLS - 1;
                else if (nextY >= GRID_COLS)  nextY = 0;
                uint8_t cell = gameGrid[nextX][nextY];

                if (cell == 1 || cell == 3) {
                    /* 1 = thân rắn, 3 = tường → đều Game Over */
                    currentState         = GAME_OVER;
                    gameOverScreenDrawn  = 0;
                    gameUIRendered       = 0;
                    break;
                }
                else if (cell == 2) {
                    /* ăn mồi */
                    score++;
                    led7_show_score(score);
                    advanceSnakeHeadTo(nextX, nextY);
                    generateFruit();
                    updateScoreUI();
                }
                else {
                    /* ô trống */
                    advanceSnakeHeadTo(nextX, nextY);
                    removeSnakeTail();
                }
                setTimer_snake(300);
            }

            if (isHomeButtonTouched()) {
                currentState = GAME_INIT;
                startScreenDrawn = 0;
                score = 0;
                lastScore = -1;
                gameUIRendered = 0;
                lcd_Fill(0, 0, 240, 320, BLACK);
                break;
            }

            if (button_read_flag) {
                setTimer_button(5);
                handleInput();
            }

            if (isPauseButtonTouched()) {
                            currentState = GAME_PAUSE;
                        }
            break;

        case GAME_MAP_SELECT:
        {
            int sel = mapSelectHandleTouch();

            if (sel >= 0 && sel <= 3) {
                selectedMap = sel;
                displayMapSelectScreen();
            }

            if (sel == 100) {  // START
                lcd_Fill(0,0,240,320,BLACK);
                currentState = GAME_PLAY;

                initializeGame();   // KHÔNG vẽ map bên trong hàm này nữa

                // vẽ map theo selectedMap
                switch (selectedMap) {
                    case 0: /* Classic: không vẽ tường */ break;
                    case 1: placeBorderWalls();   break;
                    case 2: placeObstaclePlus();  break;
                    case 3: placeMazeObstacles(); break;
                }

                setTimer_button(5);
                setTimer_snake(300);
            }
        }
        break;

        case GAME_PAUSE:
//            lcd_ShowStr(80, 150, "PAUSED", YELLOW, BLACK, 24, 1);

            // lcd on screen button and physical button
            if (isPauseButtonTouched()) {
                currentState = GAME_PLAY;
            }
            if (isHomeButtonTouched()) {
                currentState     = GAME_INIT;
                startScreenDrawn = 0;
                gameUIRendered   = 0;
                score            = 0;
                lastScore        = -1;
                lcd_Fill(0, 0, 240, 320, BLACK);
            }
            break;

        case GAME_OVER:
            /* vẽ overlay 1 lần, đợi người chơi bấm RESTART */
            if (!gameOverScreenDrawn) {
                displayGameOverScreen();
                gameOverScreenDrawn = 1;
            }

            if (isRestartTouched()) {
                currentState     = GAME_INIT;  /* theo yêu cầu: RESTART → về INIT */
                startInputLock = 1;   // [NEW] chống dính START khi vừa quay lại GameStart
                startScreenDrawn = 0; // (giữ nguyên dòng này nếu đã có)
                startScreenDrawn = 0;
                gameUIRendered   = 0;
                lastScore        = -1;
                lcd_Fill(0, 0, 240, 320, BLACK);
            }
            break;
    }
}

/* Hàm vẽ mũi tên dùng lcd_DrawLine để đảm bảo hiển thị tốt */
static void drawArrow(uint16_t cx, uint16_t cy, uint16_t btnSize, enum Direction dir, uint16_t color) {
    // [FIX 1] Tính toán lại kích thước mũi tên cho vừa vặn nút
    // Nút 20px -> r = 6px -> Mũi tên rộng 12px, cao 6px (tỉ lệ đẹp)
    uint16_t r = btnSize / 3;

    switch (dir) {
        case UP:
            // Vẽ từ đỉnh (trên) xuống đáy (dưới)
            for (int i = 0; i <= r; i++) {
                // [FIX 2] Dùng lcd_DrawLine thay vì lcd_Fill
                lcd_DrawLine(cx - i, (cy - r/2) + i, cx + i, (cy - r/2) + i, color);
            }
            break;

        case DOWN:
            // Vẽ từ đỉnh (dưới) lên đáy (trên)
            for (int i = 0; i <= r; i++) {
                lcd_DrawLine(cx - i, (cy + r/2) - i, cx + i, (cy + r/2) - i, color);
            }
            break;

        case LEFT:
            // Vẽ từ đỉnh (trái) sang đáy (phải)
            for (int i = 0; i <= r; i++) {
                lcd_DrawLine((cx - r/2) + i, cy - i, (cx - r/2) + i, cy + i, color);
            }
            break;

        case RIGHT:
            // Vẽ từ đỉnh (phải) sang đáy (trái)
            for (int i = 0; i <= r; i++) {
                lcd_DrawLine((cx + r/2) - i, cy - i, (cx + r/2) - i, cy + i, color);
            }
            break;
    }
}

void initializeControlButtons(void) {
    // Button 0: UP
    controlButtons[0] = (ControlButton){
        .xStart = DIRECTION_BTN_X + DIRECTION_BTN_SIZE + 10,
        .yStart = DIRECTION_BTN_Y + 10 + DIRECTION_BTN_SIZE,
        .xEnd   = DIRECTION_BTN_X + 2 * DIRECTION_BTN_SIZE + 10,
        .yEnd   = DIRECTION_BTN_Y + 2 * DIRECTION_BTN_SIZE + 10, // Lưu ý: code cũ của bạn nhân 2 ở yEnd có vẻ hơi lệch, tôi chỉnh lại cho vuông
        .isPressed = 0
    };
    // Button 1: DOWN
    controlButtons[1] = (ControlButton){
        .xStart = DIRECTION_BTN_X + DIRECTION_BTN_SIZE + 10,
        .yStart = DIRECTION_BTN_Y + 2 * DIRECTION_BTN_SIZE + 20,
        .xEnd   = DIRECTION_BTN_X + 2 * DIRECTION_BTN_SIZE + 10,
        .yEnd   = DIRECTION_BTN_Y + 3 * DIRECTION_BTN_SIZE + 20,
        .isPressed = 0
    };
    // Button 2: LEFT
    controlButtons[2] = (ControlButton){
        .xStart = DIRECTION_BTN_X,
        .yStart = DIRECTION_BTN_Y + 2 * DIRECTION_BTN_SIZE + 20,
        .xEnd   = DIRECTION_BTN_X + DIRECTION_BTN_SIZE,
        .yEnd   = DIRECTION_BTN_Y + 3 * DIRECTION_BTN_SIZE + 20,
        .isPressed = 0
    };
    // Button 3: RIGHT
    controlButtons[3] = (ControlButton){
        .xStart = DIRECTION_BTN_X + 2 * DIRECTION_BTN_SIZE + 20,
        .yStart = DIRECTION_BTN_Y + 2 * DIRECTION_BTN_SIZE + 20,
        .xEnd   = DIRECTION_BTN_X + 3 * DIRECTION_BTN_SIZE + 20,
        .yEnd   = DIRECTION_BTN_Y + 3 * DIRECTION_BTN_SIZE + 20,
        .isPressed = 0
    };

    // 2. Vẽ từng nút
    for (int i = 0; i < 4; i++) {
        uint16_t x1 = controlButtons[i].xStart;
        uint16_t y1 = controlButtons[i].yStart;
        uint16_t x2 = controlButtons[i].xEnd;
        uint16_t y2 = controlButtons[i].yEnd;

        // A. Vẽ nền nút (Màu trắng)
        lcd_Fill(x1, y1, x2, y2, WHITE);

        // B. Vẽ viền nút (Màu đen) - Tùy chọn, giúp nút rõ ràng hơn
        lcd_DrawRectangle(x1, y1, x2, y2, BLACK);

        // C. Tính tâm nút để vẽ mũi tên
        uint16_t centerX = (x1 + x2) / 2;
        uint16_t centerY = (y1 + y2) / 2;

        // D. Xác định hướng mũi tên dựa vào index i
        enum Direction dir;
        if (i == 0) dir = UP;
        else if (i == 1) dir = DOWN;
        else if (i == 2) dir = LEFT;
        else dir = RIGHT;

        // E. Vẽ mũi tên (Màu đen hoặc Đỏ/Xanh tùy ý)
        drawArrow(centerX, centerY, DIRECTION_BTN_SIZE, dir, BLACK);
    }
}

uint8_t isHomeButtonTouched(void) {
    return touch_IsTouched() &&
           touch_GetX() > 10 && touch_GetX() < 60 &&
           touch_GetY() > 5  && touch_GetY() < 30;
}

uint8_t isPauseButtonTouched(void) {
    return touch_IsTouched() &&
           touch_GetX() > 180 && touch_GetX() < 230 &&
           touch_GetY() > 5   && touch_GetY() < 30;
}

void initializeButtons(void) {
    initializeControlButtons();

    lcd_ShowStr(SCORE_LABEL_X, SCORE_LABEL_Y, "SCORE:", BLACK, WHITE, 16, 0);
    lcd_ShowIntNum(SCORE_NUM_X, SCORE_NUM_Y, score, 3, BLACK, WHITE, 16);

    lcd_Fill(10, 5, 60, 30, WHITE);
    lcd_DrawRectangle(10, 5, 60, 30, BLACK);
    lcd_ShowStr(15, 10, "HOME", BLACK, WHITE, 16, 0);

    lcd_Fill(180, 5, 230, 30, WHITE);
    lcd_DrawRectangle(180, 5, 230, 30, BLACK);
    lcd_ShowStr(185, 10, "PAUSE", BLACK, WHITE, 16, 0);
}

uint8_t isButtonUp(void) {
    if (touch_IsTouched() &&
        touch_GetX() > controlButtons[0].xStart && touch_GetX() < controlButtons[0].xEnd &&
        touch_GetY() > controlButtons[0].yStart && touch_GetY() < controlButtons[0].yEnd) {
        return 1;
    }
    return 0;
}

uint8_t isButtonDown(void) {
    if (touch_IsTouched() &&
        touch_GetX() > controlButtons[1].xStart && touch_GetX() < controlButtons[1].xEnd &&
        touch_GetY() > controlButtons[1].yStart && touch_GetY() < controlButtons[1].yEnd) {
        return 1;
    }
    return 0;
}

uint8_t isButtonLeft(void) {
    if (touch_IsTouched() &&
        touch_GetX() > controlButtons[2].xStart && touch_GetX() < controlButtons[2].xEnd &&
        touch_GetY() > controlButtons[2].yStart && touch_GetY() < controlButtons[2].yEnd) {
        return 1;
    }
    return 0;
}

uint8_t isButtonRight(void) {
    if (touch_IsTouched() &&
        touch_GetX() > controlButtons[3].xStart && touch_GetX() < controlButtons[3].xEnd &&
        touch_GetY() > controlButtons[3].yStart && touch_GetY() < controlButtons[3].yEnd) {
        return 1;
    }
    return 0;
}

uint8_t isStartScreenTouched(void) {
    if (!touch_IsTouched()) return 0;

    uint16_t tx = touch_GetX();
    uint16_t ty = touch_GetY();

    // [FIX] Đồng bộ tọa độ chạm với tọa độ vẽ đã sửa ở trên
    uint16_t btnTop = 165; // Vị trí Y cố định
    uint16_t btnHeight = 45;
    uint16_t btnWidth = 150; // Kích thước W cố định
    uint16_t btnX1 = (240 - btnWidth) / 2; // Căn giữa nút
    uint16_t btnX2 = btnX1 + btnWidth;
    uint16_t btnY1 = btnTop;
    uint16_t btnY2 = btnTop + btnHeight;


    if (tx > btnX1 && tx < btnX2 &&
        ty > btnY1 && ty < btnY2) {
        return 1;
    }
    return 0;
}

uint8_t isPhyButtonUpEdge(void)    { return button_pressed_edge(BTN_IDX_UP);    }
uint8_t isPhyButtonDownEdge(void)  { return button_pressed_edge(BTN_IDX_DOWN);  }
uint8_t isPhyButtonLeftEdge(void)  { return button_pressed_edge(BTN_IDX_LEFT);  }
uint8_t isPhyButtonRightEdge(void) { return button_pressed_edge(BTN_IDX_RIGHT); }

