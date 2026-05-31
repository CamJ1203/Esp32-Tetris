#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT 64
#define OLED_RESET     -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//BORDE DEL TETRIS

#define BOARD_WIDTH   10
#define BOARD_HEIGHT  18
#define CELL_SIZE      6

#define BOARD_OFFSET_X  2
#define BOARD_OFFSET_Y  1

#define POT_PIN        34   
#define POT_ROT_PIN    35   


#define POT_DEAD_LOW  1800
#define POT_DEAD_HIGH 2200


#define ROT_DEAD_LOW  0
#define ROT_DEAD_HIGH 1000

uint8_t board[BOARD_HEIGHT][BOARD_WIDTH];

const uint8_t PIECES[7][4][4] = {
  {{0,0,0,0},{1,1,1,1},{0,0,0,0},{0,0,0,0}},
  {{0,1,1,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},
  {{0,1,0,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},
  {{0,1,1,0},{1,1,0,0},{0,0,0,0},{0,0,0,0}},
  {{1,1,0,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},
  {{1,0,0,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},
  {{0,0,1,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}}
};

uint8_t currentPiece[4][4];
int currentX, currentY;
int score = 0;
int level  = 1;
bool gameOver = false;

unsigned long lastFall  = 0;
unsigned long lastMove  = 0;
unsigned long lastRot   = 0;   
int fallInterval = 600;

int lastPotZone = 1;

void rotatePiece(uint8_t piece[4][4]) {
  uint8_t tmp[4][4] = {0};
  for (int r = 0; r < 4; r++)
    for (int c = 0; c < 4; c++)
      tmp[c][3 - r] = piece[r][c];
  memcpy(piece, tmp, sizeof(tmp));
}

bool isValid(uint8_t piece[4][4], int x, int y) {
  for (int r = 0; r < 4; r++)
    for (int c = 0; c < 4; c++)
      if (piece[r][c]) {
        int nx = x + c, ny = y + r;
        if (nx < 0 || nx >= BOARD_WIDTH) return false;
        if (ny >= BOARD_HEIGHT)          return false;
        if (ny >= 0 && board[ny][nx])    return false;
      }
  return true;
}

void placePiece() {
  for (int r = 0; r < 4; r++)
    for (int c = 0; c < 4; c++)
      if (currentPiece[r][c])
        board[currentY + r][currentX + c] = 1;

  int cleared = 0;
  for (int r = BOARD_HEIGHT - 1; r >= 0; r--) {
    bool full = true;
    for (int c = 0; c < BOARD_WIDTH; c++)
      if (!board[r][c]) { full = false; break; }
    if (full) {
      cleared++;
      for (int row = r; row > 0; row--)
        memcpy(board[row], board[row - 1], BOARD_WIDTH);
      memset(board[0], 0, BOARD_WIDTH);
      r++;
    }
  }
  int pts[] = {0, 100, 300, 500, 800};
  if (cleared) score += pts[min(cleared, 4)] * level;
  level        = score / 500 + 1;
  fallInterval = max(150, 600 - (level - 1) * 80);
}

void spawnPiece() {
  memcpy(currentPiece, PIECES[random(7)], sizeof(currentPiece));
  currentX = BOARD_WIDTH / 2 - 2;
  currentY = 0;
  if (!isValid(currentPiece, currentX, currentY))
    gameOver = true;
}

void drawBoard() {
  display.clearDisplay();

  display.drawRect(
    BOARD_OFFSET_X - 1,
    BOARD_OFFSET_Y - 1,
    BOARD_WIDTH * CELL_SIZE + 2,
    BOARD_HEIGHT * CELL_SIZE + 2,
    WHITE
  );

  for (int r = 0; r < BOARD_HEIGHT; r++)
    for (int c = 0; c < BOARD_WIDTH; c++)
      if (board[r][c])
        display.fillRect(
          BOARD_OFFSET_X + c * CELL_SIZE,
          BOARD_OFFSET_Y + r * CELL_SIZE,
          CELL_SIZE - 1, CELL_SIZE - 1,
          WHITE
        );

  for (int r = 0; r < 4; r++)
    for (int c = 0; c < 4; c++)
      if (currentPiece[r][c]) {
        int dx = BOARD_OFFSET_X + (currentX + c) * CELL_SIZE;
        int dy = BOARD_OFFSET_Y + (currentY + r) * CELL_SIZE;
        if (dy >= BOARD_OFFSET_Y)
          display.fillRect(dx, dy, CELL_SIZE - 1, CELL_SIZE - 1, WHITE);
      }

  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 119);
  display.print("S:");
  display.print(score);
  display.print(" L:");
  display.print(level);

  display.display();
}

void setup() {
  Serial.begin(115200);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED no encontrada");
    for (;;);
  }

  display.setRotation(1);
  display.clearDisplay();
  display.display();

  memset(board, 0, sizeof(board));
  randomSeed(analogRead(36));   
  spawnPiece();
}

void loop() {
  if (gameOver) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(8, 44);
    display.print("GAME OVER");
    display.setCursor(8, 58);
    display.print("SC: ");
    display.print(score);
    display.display();
    delay(3000);
    memset(board, 0, sizeof(board));
    score = 0; level = 1; fallInterval = 600;
    gameOver = false;
    spawnPiece();
    return;
  }

  unsigned long now = millis();


  // POT 1 — MOVIMIENTO LATERAL 

  int potVal  = analogRead(POT_PIN);
  int potZone = (potVal < POT_DEAD_LOW) ? 0 : (potVal > POT_DEAD_HIGH) ? 2 : 1;

  if (now - lastMove > 500) {
    if (potZone == 0 && isValid(currentPiece, currentX - 1, currentY)) {
      currentX--;
      lastMove = now;
    } else if (potZone == 2 && isValid(currentPiece, currentX + 1, currentY)) {
      currentX++;
      lastMove = now;
    }
  }

  // POT 2 — ROTACIÓN

  int rotVal  = analogRead(POT_ROT_PIN);
  bool wantsRot = (rotVal < ROT_DEAD_LOW || rotVal > ROT_DEAD_HIGH);

  if (wantsRot && now - lastRot > 600) {
    lastRot = now;

    // Copiar pieza actual, rotar la copia
    uint8_t rotated[4][4];
    memcpy(rotated, currentPiece, sizeof(rotated));
    rotatePiece(rotated);

    // Intentar colocar en posición actual, luego ±1, luego ±2 (wall kick)
    int kicks[] = {0, -1, 1, -2, 2};
    for (int i = 0; i < 5; i++) {
      if (isValid(rotated, currentX + kicks[i], currentY)) {
        memcpy(currentPiece, rotated, sizeof(currentPiece));
        currentX += kicks[i];
        break;
      }
    }
    // Si ningún kick funciona, la rotación simplemente no se aplica
  }


  // CAÍDA AUTOMÁTICA
 
  if (now - lastFall >= (unsigned long)fallInterval) {
    lastFall = now;
    if (isValid(currentPiece, currentX, currentY + 1))
      currentY++;
    else {
      placePiece();
      spawnPiece();
    }
  }

  drawBoard();
  delay(10);
}