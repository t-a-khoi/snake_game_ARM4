/*
 * game_display.c
 *  Nền đen, viền trắng 1px. Vẽ incremental, không lưới, không giật.
 */

#include "game_display.h"
#include <stdlib.h>
#include <string.h>

struct Fruit {
    uint16_t x, y;
    uint16_t color;
} fruit;

struct Snake snake;
enum Direction snakeDirection = DOWN;

uint8_t  gameGrid[GRID_ROWS][GRID_COLS];
static int16_t prevX[GRID_ROWS][GRID_COLS];
static int16_t prevY[GRID_ROWS][GRID_COLS];

/* ========= KHUNG & VÙNG VẼ =========
   - Khung (frame) trắng 1px nằm ở biên SCREEN_X..SCREEN_X+SCREEN_SIZE
   - Vùng vẽ PLAY_* là phần lõm vào 1px (để ô không chạm viền)        */
#define FRAME_COLOR   WHITE
#define PLAY_X        (SCREEN_X + 1)
#define PLAY_Y        (SCREEN_Y + 1)
#define PLAY_SIZE     (SCREEN_SIZE - 2)
/* Ô cuối cùng sẽ kết thúc tại: PLAY_X + PLAY_SIZE - 1 (lọt trong khung) */

static inline void drawPlayfieldFrame(void) {
    lcd_DrawRectangle(SCREEN_X, SCREEN_Y,
                      SCREEN_X + SCREEN_SIZE, SCREEN_Y + SCREEN_SIZE,
                      FRAME_COLOR);
}

/* Vẽ 1 ô GRID (i,j) trong vùng PLAY, clamp biên an toàn */
static inline void drawCell(uint8_t i, uint8_t j, uint16_t color) {
    // Pixel bắt đầu của ô
    uint16_t x1 = PLAY_X + i * CELL_SIZE;
    uint16_t y1 = PLAY_Y + j * CELL_SIZE;

    // Pixel kết thúc của ô (bao gồm) nhưng không vượt quá vùng PLAY
    uint16_t x2 = x1 + CELL_SIZE - 1;
    uint16_t y2 = y1 + CELL_SIZE - 1;

    // Clamp để đảm bảo KHÔNG chạm viền ngoài (tránh đè viền)
    uint16_t maxX = PLAY_X + PLAY_SIZE - 1;
    uint16_t maxY = PLAY_Y + PLAY_SIZE - 1;
    if (x2 > maxX) x2 = maxX;
    if (y2 > maxY) y2 = maxY;

    // Nếu vì cấu hình CELL_SIZE không chia hết, ô mép có thể co lại 1px — chấp nhận.
    lcd_Fill(x1, y1, x2, y2, color);
}

/* Không redraw toàn màn để tránh giật */
void renderScreen(void) {
    // intentionally empty — vẽ incremental ở advance/remove/generate
}

void generateFruit(void) {
    while (1) {
        uint16_t x = 1 + rand() % (GRID_ROWS - 2);
        uint16_t y = 1 + rand() % (GRID_COLS - 2);

        if (gameGrid[x][y] == 0) {   // chỉ spawn vào ô trống
            fruit.x = x;
            fruit.y = y;
            gameGrid[x][y] = 2;
            drawCell(x, y, RED);
            break;
        }
    }
}

void generateSafeSnakeStart(uint16_t *hx, uint16_t *hy,
                            uint16_t *tx, uint16_t *ty,
                            enum Direction *dir)
{
    while (1) {
        // tránh viền: 1..GRID-2
        uint16_t x = 1 + rand() % (GRID_ROWS - 2);
        uint16_t y = 1 + rand() % (GRID_COLS - 2);

        // ô đầu phải trống
        if (gameGrid[x][y] != 0) continue;

        // random hướng 0..3
        int d = rand() % 4;
        int dx = 0, dy = 0;

        switch (d) {
            case 0: dx = -1; dy = 0;  *dir = LEFT;  break;
            case 1: dx =  1; dy = 0;  *dir = RIGHT; break;
            case 2: dx = 0;  dy = -1; *dir = UP;    break;
            case 3: dx = 0;  dy =  1; *dir = DOWN;  break;
        }

        // tail = ngược hướng head
        int txPos = x - dx;
        int tyPos = y - dy;

        // tail phải nằm trong map
        if (txPos < 1 || txPos > GRID_ROWS - 2) continue;
        if (tyPos < 1 || tyPos > GRID_COLS - 2) continue;

        // tail không trùng obstacle hoặc fruit
        if (gameGrid[txPos][tyPos] != 0) continue;

        // hợp lệ -> trả dữ liệu
        *hx = x;
        *hy = y;
        *tx = txPos;
        *ty = tyPos;
        return;
    }
}

void initializeGame(void) {
    memset(gameGrid, 0, sizeof(gameGrid));
    for (uint8_t i = 0; i < GRID_ROWS; ++i)
        for (uint8_t j = 0; j < GRID_COLS; ++j)
            prevX[i][j] = prevY[i][j] = -1;

    // nền vùng chơi
    lcd_Fill(PLAY_X, PLAY_Y, PLAY_X + PLAY_SIZE, PLAY_Y + PLAY_SIZE, BLACK);

    // Viền LCD (frame trắng)
    drawPlayfieldFrame();

    // ==== Viền map hoặc Maze trước ====
//    placeBorderWalls();
//    placeMazeObstacles();   // nếu có maze bên trong
    // ==== Random vị trí + random hướng ====
    uint16_t hx, hy, tx, ty;
    enum Direction dir;

    generateSafeSnakeStart(&hx, &hy, &tx, &ty, &dir);

    snake.headX = hx;
    snake.headY = hy;
    snake.tailX = tx;
    snake.tailY = ty;
    snakeDirection = dir;   // <<<< RANDOM HƯỚNG

    gameGrid[tx][ty] = 1;
    gameGrid[hx][hy] = 1;

    prevX[hx][hy] = tx;
    prevY[hx][hy] = ty;
    prevX[tx][ty] = -1;
    prevY[tx][ty] = -1;

    // Vẽ rắn
    {
        uint16_t snakeColor = (snake.color ? snake.color : GREEN);
        drawCell(tx, ty, snakeColor);
        drawCell(hx, hy, snakeColor);
    }

    // ==== Spawn mồi tránh viền + tránh tường ====
    fruit.color = RED;
    generateFruit();
}


void placeObstaclePlus(void) {
    uint8_t cx = GRID_ROWS / 2 - 1;   // tâm
    uint8_t cy = GRID_COLS / 2 - 1;

    // Dấu cộng ngang
    for (int i = -4; i <= 4; i++) {
        gameGrid[cx + i][cy] = 3;
        drawCell(cx + i, cy, GBLUE);
    }

    // Dấu cộng dọc
    for (int j = -4; j <= 4; j++) {
        gameGrid[cx][cy + j] = 3;
        drawCell(cx, cy + j, GBLUE);
    }
}

void placeMazeObstacles(void) {
    // ===== THANH DỌC BÊN TRÁI =====
    for (int y = 2; y <= 12; y++) {
        gameGrid[2][y] = 3;
        drawCell(2, y, BRRED);
    }

    // ===== THANH DỌC BÊN PHẢI =====
    for (int y = 5; y <= 12; y++) {
        gameGrid[13][y] = 3;
        drawCell(13, y, BRRED);
    }

    // ===== THANH NGANG GIỮA =====
    for (int x = 4; x <= 12; x++) {
        gameGrid[x][8] = 3;
        drawCell(x, 8, BRRED);
    }

    // ===== THANH DỌC NGẮN BÊN DƯỚI =====
    for (int y = 12; y <= 15; y++) {
        gameGrid[7][y] = 3;
        drawCell(7, y, BRRED);
    }

    // ===== THANH NGANG DƯỚI =====
    for (int x = 3; x <= 12; x++) {
        gameGrid[x][15] = 3;
        drawCell(x, 15, BRRED);
    }
}

void placeBorderWalls(void) {
    // === Top border (viền trên) ===
    for (int x = 0; x < GRID_ROWS; x++) {
        gameGrid[x][0] = 3;
        drawCell(x, 0, MAGENTA );
    }

    // === Bottom border (viền dưới) ===
    for (int x = 0; x < GRID_ROWS; x++) {
        gameGrid[x][GRID_COLS - 1] = 3;
        drawCell(x, GRID_COLS - 1, MAGENTA );
    }

    // === Left border (viền trái) ===
    for (int y = 0; y < GRID_COLS; y++) {
        gameGrid[0][y] = 3;
        drawCell(0, y, MAGENTA );
    }

    // === Right border (viền phải) ===
    for (int y = 0; y < GRID_COLS; y++) {
        gameGrid[GRID_ROWS - 1][y] = 3;
        drawCell(GRID_ROWS - 1, y, MAGENTA);
    }
}




/* Không dùng trong FSM hiện tại, giữ lại tham khảo */
void advanceSnakeHead(void) {
    uint16_t oldHeadX = snake.headX;
    uint16_t oldHeadY = snake.headY;

    switch (snakeDirection) {
        case UP:    snake.headY--; break;
        case DOWN:  snake.headY++; break;
        case LEFT:  snake.headX--; break;
        case RIGHT: snake.headX++; break;
    }

    if (snake.headX < 0) snake.headX = GRID_ROWS - 1;
    if (snake.headX >= GRID_ROWS) snake.headX = 0;
    if (snake.headY < 0) snake.headY = GRID_COLS - 1;
    if (snake.headY >= GRID_COLS) snake.headY = 0;

    prevX[snake.headX][snake.headY] = oldHeadX;
    prevY[snake.headX][snake.headY] = oldHeadY;

    gameGrid[snake.headX][snake.headY] = 1;

    drawCell(snake.headX, snake.headY, (snake.color ? snake.color : GREEN));
}

void advanceSnakeHeadTo(int16_t nx, int16_t ny) {
    uint16_t oldHeadX = snake.headX;
    uint16_t oldHeadY = snake.headY;

    snake.headX = (uint16_t)nx;
    snake.headY = (uint16_t)ny;

    prevX[snake.headX][snake.headY] = oldHeadX;
    prevY[snake.headX][snake.headY] = oldHeadY;

    gameGrid[snake.headX][snake.headY] = 1;

    drawCell(snake.headX, snake.headY, (snake.color ? snake.color : GREEN));
}

void removeSnakeTail(void) {
    uint16_t curTailX = snake.tailX;
    uint16_t curTailY = snake.tailY;

    int16_t nextTailX = -1;
    int16_t nextTailY = -1;

    for (uint8_t i = 0; i < GRID_ROWS; i++) {
        for (uint8_t j = 0; j < GRID_COLS; j++) {
            if (prevX[i][j] == curTailX && prevY[i][j] == curTailY) {
                nextTailX = i;
                nextTailY = j;
                break;
            }
        }
        if (nextTailX != -1) break;
    }

    gameGrid[curTailX][curTailY] = 0;
    drawCell(curTailX, curTailY, BLACK);  // trả lại nền đen (không đụng viền)

    prevX[curTailX][curTailY] = -1;
    prevY[curTailX][curTailY] = -1;

    if (nextTailX != -1) {
        snake.tailX = (uint16_t)nextTailX;
        snake.tailY = (uint16_t)nextTailY;
    } else {
        snake.tailX = snake.headX;
        snake.tailY = snake.headY;
    }
}

/* Input giữ nguyên */
void handleInput(void) {
    if ((isButtonLeft() || isPhyButtonLeftEdge()) && (snakeDirection == UP || snakeDirection == DOWN)) {
        snakeDirection = LEFT;
    } else if ((isButtonRight() || isPhyButtonRightEdge()) && (snakeDirection == UP || snakeDirection == DOWN)) {
        snakeDirection = RIGHT;
    } else if ((isButtonUp() || isPhyButtonUpEdge()) && (snakeDirection == LEFT || snakeDirection == RIGHT)) {
        snakeDirection = UP;
    } else if ((isButtonDown() || isPhyButtonDownEdge()) && (snakeDirection == LEFT || snakeDirection == RIGHT)) {
        snakeDirection = DOWN;
    }
}

/* Start screen giữ nguyên như trước */
void displayStartScreen(void) {
    lcd_Fill(0, 0, 240, 320, BLACK);

    // [FIX] Căn giữa chữ "SNAKE GAME" (font 24) theo chiều rộng 240px
    lcd_ShowStr(55, 50, "SNAKE GAME", WHITE, BLACK, 24, 0);

    // [FIX] Căn giữa chữ "Choose color" (font 16)
    lcd_ShowStr(70, 85, "Choose color", WHITE, BLACK, 16, 0);

    // [FIX] Cố định vị trí Y và căn giữa cụm 4 box màu
    uint16_t top  = 110; // Vị trí Y cố định
    uint16_t w    = 26;
    uint16_t h    = 25;
    uint16_t gap  = 10;
    uint16_t total_w = (w * 4) + (gap * 3);
    uint16_t left = (240 - total_w) / 2; // Căn giữa cụm box

    // Box 1 (GREEN)
    lcd_Fill(left, top, left + w, top + h, GREEN);
    lcd_Fill(left-1, top-1, left + w+1, top+1, WHITE);
    lcd_Fill(left-1, top+h-1, left + w+1, top+h+1, WHITE);
    lcd_Fill(left-1, top-1, left+1, top+h+1, WHITE);
    lcd_Fill(left+w-1, top-1, left+w+1, top+h+1, WHITE);

    // Box 2 (BLUE)
    uint16_t x2 = left + (w + gap);
    lcd_Fill(x2, top, x2 + w, top + h, BLUE);
    lcd_Fill(x2-1, top-1, x2 + w+1, top+1, WHITE);
    lcd_Fill(x2-1, top+h-1, x2 + w+1, top+h+1, WHITE);
    lcd_Fill(x2-1, top-1, x2+1, top+h+1, WHITE);
    lcd_Fill(x2+w-1, top-1, x2+w+1, top+h+1, WHITE);

    // Box 3 (MAGENTA)
    uint16_t x3 = left + (w + gap)*2;
    lcd_Fill(x3, top, x3 + w, top + h, MAGENTA);
    lcd_Fill(x3-1, top-1, x3 + w+1, top+1, WHITE);
    lcd_Fill(x3-1, top+h-1, x3 + w+1, top+h+1, WHITE);
    lcd_Fill(x3-1, top-1, x3+1, top+h+1, WHITE);
    lcd_Fill(x3+w-1, top-1, x3+w+1, top+h+1, WHITE);

    // Box 4 (YELLOW)
    uint16_t x4 = left + (w + gap)*3;
    lcd_Fill(x4, top, x4 + w, top + h, YELLOW);
    lcd_Fill(x4-1, top-1, x4 + w+1, top+1, WHITE);
    lcd_Fill(x4-1, top+h-1, x4 + w+1, top+h+1, WHITE);
    lcd_Fill(x4-1, top-1, x4+1, top+h+1, WHITE);
    lcd_Fill(x4+w-1, top-1, x4+w+1, top+h+1, WHITE);

    // [FIX] Cố định vị trí và kích thước nút START
    uint16_t btnTop = 165; // Vị trí Y cố định
    uint16_t btnHeight = 45;
    uint16_t btnWidth = 150; // Kích thước W cố định
    uint16_t btnX1 = (240 - btnWidth) / 2; // Căn giữa nút
    uint16_t btnX2 = btnX1 + btnWidth;

    lcd_Fill(btnX1, btnTop, btnX2, btnTop + btnHeight, GREEN);
    lcd_Fill(btnX1, btnTop, btnX2, btnTop + 2, WHITE);
    lcd_Fill(btnX1, btnTop + btnHeight - 2, btnX2, btnTop + btnHeight, WHITE);
    lcd_Fill(btnX1, btnTop, btnX1 + 2, btnTop + btnHeight, WHITE);
    lcd_Fill(btnX2 - 2, btnTop, btnX2, btnTop + btnHeight, WHITE);

    // [FIX] Căn giữa chữ "START" (font 24) bên trong nút
    lcd_ShowStr(btnX1 + 45, btnTop + 10, "START", WHITE, GREEN, 24, 1);

    // [FIX] Căn giữa phần preview
    uint16_t snakePreviewY = btnTop + btnHeight + 15; // Ngay dưới nút START
    uint16_t snakePreviewX = (240 - (3 * 15)) / 2;    // Căn giữa 3 ô preview
    uint16_t previewColor = (snake.color ? snake.color : GREEN);

    for (int i = 0; i < 3; i++) {
        uint16_t x1 = snakePreviewX + i * 15;
        uint16_t y1 = snakePreviewY;
        lcd_Fill(x1, y1, x1 + 13, y1 + 13, previewColor);
        lcd_DrawRectangle(x1, y1, x1 + 13, y1 + 13, WHITE);
    }

    lcd_ShowStr(snakePreviewX - 10, snakePreviewY + 18, "Preview", WHITE, BLACK, 12, 0);
}

uint16_t startScreenHandleColorTouch(void) {
    if (!touch_IsTouched()) return 0;

    uint16_t tx = touch_GetX();
    uint16_t ty = touch_GetY();

    // [FIX] Đồng bộ tọa độ chạm với tọa độ vẽ đã sửa ở trên
    uint16_t top  = 110; // Vị trí Y cố định
    uint16_t w    = 26;
    uint16_t h    = 25;
    uint16_t gap  = 10;
    uint16_t total_w = (w * 4) + (gap * 3);
    uint16_t left = (240 - total_w) / 2; // Căn giữa cụm box

    if (tx > left && tx < left + w &&
        ty > top  && ty < top + h) {
        return GREEN;
    }

    uint16_t x2 = left + (w + gap);
    if (tx > x2 && tx < x2 + w &&
        ty > top && ty < top + h) {
        return BLUE;
    }

    uint16_t x3 = left + (w + gap) * 2;
    if (tx > x3 && tx < x3 + w &&
        ty > top && ty < top + h) {
        return MAGENTA;
    }

    uint16_t x4 = left + (w + gap) * 3;
    if (tx > x4 && tx < x4 + w &&
        ty > top && ty < top + h) {
        return YELLOW;
    }
    return 0;
}

static void drawMapPreview(uint8_t mapId,
                           uint16_t x1, uint16_t y1,
                           uint16_t x2, uint16_t y2,
                           uint8_t selected)
{
    uint16_t borderColor = selected ? RED : WHITE;

    // viền khung preview
    lcd_DrawRectangle(x1, y1, x2, y2, borderColor);

    // nền trong khung
    lcd_Fill(x1+1, y1+1, x2-1, y2-1, BLACK);

    uint16_t px1 = x1 + 5;
    uint16_t py1 = y1 + 5;
    uint16_t px2 = x2 - 5;
    uint16_t py2 = y2 - 5;

    uint16_t midX = (px1 + px2) / 2;
    uint16_t midY = (py1 + py2) / 2;

    // MAP PREVIEW
    switch (mapId) {
    case 0: // CLASSIC
        lcd_DrawRectangle(px1, py1, px2, py2, BRRED);
        break;

    case 1: // BORDER
        lcd_Fill(px1, py1, px2, py1+2, MAGENTA );        // top
        lcd_Fill(px1, py2-2, px2, py2, MAGENTA );        // bottom
        lcd_Fill(px1, py1, px1+2, py2, MAGENTA );        // left
        lcd_Fill(px2-2, py1, px2, py2, MAGENTA );        // right
        break;

    case 2: // PLUS
        lcd_Fill(px1, midY-1, px2, midY+1, GBLUE);
        lcd_Fill(midX-1, py1, midX+1, py2, GBLUE);
        break;

    case 3: // MAZE
        lcd_Fill(px1+3, py1, px1+5, py1+(py2-py1)*2/3, BRRED);
        lcd_Fill(px2-5, py1+(py2-py1)/4, px2-3, py2, BRRED);
        lcd_Fill(px1+8, midY-1, px2-8, midY+1, BRRED);
        lcd_Fill(midX-1, midY, midX+1, py2-5, BRRED);
        lcd_Fill(px1+6, py2-4, px2-12, py2-2, BRRED);
        break;
    }
}


void displayMapSelectScreen(void) {
    lcd_Fill(0,0,240,320,BLACK);
    lcd_ShowStr(40, 10, "SELECT MAP", WHITE, BLACK, 24, 0);

    uint16_t w = 90, h = 60;

    uint16_t m0_x1 = 20,     m0_y1 = 50;
    uint16_t m0_x2 = m0_x1+w, m0_y2 = m0_y1+h;
    drawMapPreview(0, m0_x1, m0_y1, m0_x2, m0_y2, selectedMap==0);
    lcd_ShowStr(m0_x1+15, m0_y2+5, "CLASSIC", WHITE, BLACK, 12, 0);

    uint16_t m1_x1 = 130,     m1_y1 = 50;
    uint16_t m1_x2 = m1_x1+w, m1_y2 = m1_y1+h;
    drawMapPreview(1, m1_x1, m1_y1, m1_x2, m1_y2, selectedMap==1);
    lcd_ShowStr(m1_x1+20, m1_y2+5, "BORDER", WHITE, BLACK, 12, 0);

    uint16_t m2_x1 = 20,      m2_y1 = 140;
    uint16_t m2_x2 = m2_x1+w, m2_y2 = m2_y1+h;
    drawMapPreview(2, m2_x1, m2_y1, m2_x2, m2_y2, selectedMap==2);
    lcd_ShowStr(m2_x1+25, m2_y2+5, "PLUS", WHITE, BLACK, 12, 0);

    uint16_t m3_x1 = 130,      m3_y1 = 140;
    uint16_t m3_x2 = m3_x1+w,  m3_y2 = m3_y1+h;
    drawMapPreview(3, m3_x1, m3_y1, m3_x2, m3_y2, selectedMap==3);
    lcd_ShowStr(m3_x1+28, m3_y2+5, "MAZE", WHITE, BLACK, 12, 0);

    lcd_Fill(50, 260, 190, 300, GREEN);
    lcd_DrawRectangle(50, 260, 190, 300, WHITE);
    lcd_ShowStr(95, 272, "START", WHITE, GREEN, 24, 1);
}



int mapSelectHandleTouch(void) {
    if (!touch_IsTouched()) return -1;

    uint16_t x = touch_GetX();
    uint16_t y = touch_GetY();

    // Map 0 box: (20,50)-(110,110)
    if (x>20 && x<110 && y>50 && y<110) return 0;
    // Map 1 box: (130,50)-(220,110)
    if (x>130 && x<220 && y>50 && y<110) return 1;
    // Map 2 box: (20,140)-(110,200)
    if (x>20 && x<110 && y>140 && y<200) return 2;
    // Map 3 box: (130,140)-(220,200)
    if (x>130 && x<220 && y>140 && y<200) return 3;

    // START button: (50,260)-(190,300)
    if (x>50 && x<190 && y>260 && y<300)
        return 100;

    return -1;
}


