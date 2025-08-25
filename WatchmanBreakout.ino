/*
  Breakout game for Sony Watchman
  Sideburn Studios - August 2025
  Converted from Pong template
*/

#include <avr/pgmspace.h>
#include <TVout.h>
#include <video_gen.h>
#include <Controllers.h>

#define W 135
#define H 98
#define GAME_START 10  // D10 switch for attract mode control

// Breakout variables
int ballx, bally;
char dx;
char dy;
byte paddleX = 60;        // Player paddle X position (centered)
byte paddleY = H - 8;     // Player paddle Y position (near bottom)
byte paddleWidth = 16;    // Wider paddle for Breakout
byte paddleHeight = 3;    // Thinner paddle for Breakout
byte lives = 3;           // Player lives
byte level = 1;           // Current level
boolean attractMode = false;
boolean gameEnded = false;
boolean lastSwitchState = HIGH;

// Brick system
#define BRICK_ROWS 6
#define BRICK_COLS 12
#define BRICK_WIDTH 10
#define BRICK_HEIGHT 4
#define BRICK_START_Y 15
#define BRICK_SPACING_X 1
#define BRICK_SPACING_Y 1
boolean bricks[BRICK_ROWS][BRICK_COLS];
int bricksRemaining = 0;

// Player paddle control variables
#define BUFFER_SIZE 8  // Smaller buffer for more responsive breakout paddle
int paddleBuffer[BUFFER_SIZE];
byte bufferIndex = 0;
boolean bufferFilled = false;
byte lastPaddleX = 60;

// Attract mode AI paddle
float aiPaddleTarget = 60.0;
float aiPaddleFloat = 60.0;
float aiMaxSpeed = 2.0;
float aiMomentum = 0.0;

TVout tv;

void setup() {  
  pinMode(GAME_START, INPUT_PULLUP);
  tv.begin(NTSC, W, H);
  randomSeed(analogRead(0));

  // Startup tones
  playTone(100, 20);
  tv.delay(16);
  playTone(200, 20);
  tv.delay(16);
  playTone(400, 20);
  tv.delay(16);
  playTone(800, 20);
  tv.delay(160);
  
  // Initialize paddle smoothing buffer
  for(int i = 0; i < BUFFER_SIZE; i++) {
    paddleBuffer[i] = 855; // Initialize to center value
  }

  // Show intro splash screen
  drawIntroScreen();
  
  initBreakout();
}

void loop() {
  bool currentSwitch = (digitalRead(GAME_START) == LOW);
  
  // Detect switch change
  if (currentSwitch != lastSwitchState) {
    if (currentSwitch) {
      // Switched into attract mode
      tv.delay(1000);
      initBreakout();
    }
  }
  lastSwitchState = currentSwitch;

  breakout();
}

void breakout() {
  // Check attract mode switch (D10)
  attractMode = (digitalRead(GAME_START) == HIGH);
  if(attractMode){
    gameEnded = false;
  } 

  // Game ended, switch to attract mode
  if(gameEnded == true){
    attractMode = true;
  }
  
  drawBricks();
  drawPaddle();
  drawUI();

  if (dy == 0) {
    // Ball auto-serves upward
    if (random(0, 2) == 0) {
      dy = -1;  // Start going up
    } else {
      dy = -2;  // Start going up faster
    }
    dx = random(0, 2) == 0 ? 1 : -1;  // Random horizontal direction
  }

  // Erase old ball position
  tv.set_pixel(ballx, bally, 0);
  tv.set_pixel(ballx + 1, bally, 0);
  tv.set_pixel(ballx, bally + 1, 0);
  tv.set_pixel(ballx + 1, bally + 1, 0);

  moveBall();

  // Draw new ball position
  tv.set_pixel(ballx, bally, 1);
  tv.set_pixel(ballx + 1, bally, 1);
  tv.set_pixel(ballx, bally + 1, 1);
  tv.set_pixel(ballx + 1, bally + 1, 1);

  tv.delay(20);  // Slightly slower than Pong for better brick collision detection
}

void drawPaddle() {
  if (attractMode) {
    // AI controls paddle in attract mode
    updateAIPaddle();
  } else {
    // Human player controls paddle
    int rawValue = analogRead(A3);
    int minReading = 815;
    int maxReading = 895;
    
    rawValue = constrain(rawValue, minReading, maxReading);
    
    // Add to circular buffer for smoothing
    paddleBuffer[bufferIndex] = rawValue;
    bufferIndex = (bufferIndex + 1) % BUFFER_SIZE;
    if (!bufferFilled && bufferIndex == 0) {
      bufferFilled = true;
    }
    
    // Calculate moving average
    long sum = 0;
    int samplesUsed = bufferFilled ? BUFFER_SIZE : bufferIndex;
    for(int i = 0; i < samplesUsed; i++) {
      sum += paddleBuffer[i];
    }
    int smoothedValue = sum / samplesUsed;
    
    // Map to paddle X position (leave room for paddle width)
    byte newPaddleX = map(smoothedValue, minReading, maxReading, 0, W - paddleWidth);
    
    // Add small deadzone for stability
    if (abs(newPaddleX - lastPaddleX) > 0) {  
      paddleX = newPaddleX;
      lastPaddleX = paddleX;
    }
  }
  
  // Boundary checking
  if(paddleX < 0) paddleX = 0;
  if(paddleX > W - paddleWidth) paddleX = W - paddleWidth;
  
  // Clear paddle area first
  for (int x = 0; x < W; x++) {
    for (int y = paddleY; y < paddleY + paddleHeight; y++) {
      tv.set_pixel(x, y, 0);
    }
  }
  
  // Draw paddle
  for (int x = paddleX; x < paddleX + paddleWidth; x++) {
    for (int y = paddleY; y < paddleY + paddleHeight; y++) {
      tv.set_pixel(x, y, 1);
    }
  }
}

void moveBall() {
  // Ball went off bottom - lose a life
  if (bally >= H - 1) {
    if (!attractMode) {
      lives--;
      drawUI();
      missSound();
      tv.delay(1000);
      
      if (lives <= 0) {
        gameOver();
        return;
      }
    }
    
    // Respawn ball
    ballx = paddleX + (paddleWidth / 2);
    bally = paddleY - 5;
    dx = random(0, 2) == 0 ? 1 : -1;
    dy = 0;  // Will auto-serve next frame
    return;
  }

  // Ball hit left or right walls
  if (ballx <= 0 || ballx >= W - 2) {
    dx = -dx;
    bounceSound();
  }

  // Ball hit top wall
  if (bally <= 0) {
    dy = -dy;
    bounceSound();
  }

  // Check paddle collision
  if (bally + 2 >= paddleY && bally <= paddleY + paddleHeight) {
    if (ballx + 2 >= paddleX && ballx <= paddleX + paddleWidth) {
      dy = -abs(dy);  // Always bounce up
      
      // Calculate hit position on paddle for angle
      float hitPosition = (float)(ballx - paddleX) / (float)paddleWidth;
      
      // Adjust horizontal speed based on hit position
      if (hitPosition < 0.3) {
        dx = -2;  // Hit left side, go left fast
      } else if (hitPosition < 0.4) {
        dx = -1;  // Hit left-center, go left
      } else if (hitPosition < 0.6) {
        // Center hit - keep current direction but maybe slow down
        if (abs(dx) > 1) dx = dx > 0 ? 1 : -1;
      } else if (hitPosition < 0.7) {
        dx = 1;   // Hit right-center, go right
      } else {
        dx = 2;   // Hit right side, go right fast
      }
      
      hitSound();
    }
  }

  // Check brick collisions
  checkBrickCollisions();

  // Apply movement
  ballx += dx;
  bally += dy;

  // Keep ball in bounds
  if (ballx < 0) ballx = 0;
  if (ballx >= W - 1) ballx = W - 2;
  if (bally < 0) bally = 0;
}

void checkBrickCollisions() {
  // Check if ball hits any brick
  int brickCol = (ballx - 2) / (BRICK_WIDTH + BRICK_SPACING_X);
  int brickRow = (bally - BRICK_START_Y) / (BRICK_HEIGHT + BRICK_SPACING_Y);
  
  // Check collision with 2x2 ball against brick grid
  for (int checkX = ballx; checkX <= ballx + 1; checkX++) {
    for (int checkY = bally; checkY <= bally + 1; checkY++) {
      int col = (checkX - 2) / (BRICK_WIDTH + BRICK_SPACING_X);
      int row = (checkY - BRICK_START_Y) / (BRICK_HEIGHT + BRICK_SPACING_Y);
      
      if (row >= 0 && row < BRICK_ROWS && col >= 0 && col < BRICK_COLS) {
        if (bricks[row][col]) {
          // Hit a brick!
          bricks[row][col] = false;
          bricksRemaining--;
          
          // Determine bounce direction based on which side of brick was hit
          int brickPixelX = 2 + col * (BRICK_WIDTH + BRICK_SPACING_X);
          int brickPixelY = BRICK_START_Y + row * (BRICK_HEIGHT + BRICK_SPACING_Y);
          
          // Simple bounce logic - reverse the component that caused the collision
          if (checkX <= brickPixelX || checkX >= brickPixelX + BRICK_WIDTH - 1) {
            dx = -dx;  // Hit left or right side
          }
          if (checkY <= brickPixelY || checkY >= brickPixelY + BRICK_HEIGHT - 1) {
            dy = -dy;  // Hit top or bottom
          }
          
          brickSound();
          
          // Check for level complete
          if (bricksRemaining <= 0) {
            levelComplete();
            return;
          }
          
          return; // Only destroy one brick per frame
        }
      }
    }
  }
}

void drawBricks() {
  for (int row = 0; row < BRICK_ROWS; row++) {
    for (int col = 0; col < BRICK_COLS; col++) {
      if (bricks[row][col]) {
        int x = 2 + col * (BRICK_WIDTH + BRICK_SPACING_X);
        int y = BRICK_START_Y + row * (BRICK_HEIGHT + BRICK_SPACING_Y);
        
        // Draw brick
        for (int bx = x; bx < x + BRICK_WIDTH; bx++) {
          for (int by = y; by < y + BRICK_HEIGHT; by++) {
            tv.set_pixel(bx, by, 1);
          }
        }
      }
    }
  }
}

void drawUI() {
  // Clear UI area
  for (int x = 0; x < W; x++) {
    for (int y = H - 10; y < H; y++) {
      tv.set_pixel(x, y, 0);
    }
  }
  
  // Draw lives as small paddle icons on the left
  for (int i = 0; i < lives; i++) {
    int x = 5 + (i * 12);  // Space them out
    int y = H - 8;
    // Draw small paddle icon (6x2 pixels)
    for (int px = x; px < x + 6; px++) {
      tv.set_pixel(px, y, 1);
      tv.set_pixel(px, y + 1, 1);
    }
  }
  
  // Draw level as dots on the right (up to 9 levels visible)
  int dotsToShow = min(level, 9);
  for (int i = 0; i < dotsToShow; i++) {
    int x = W - 25 + (i * 3);  // Space dots out
    int y = H - 6;
    // Draw 2x2 dot
    tv.set_pixel(x, y, 1);
    tv.set_pixel(x + 1, y, 1);
    tv.set_pixel(x, y + 1, 1);
    tv.set_pixel(x + 1, y + 1, 1);
  }
  
  // If level > 9, draw a larger indicator
  if (level > 9) {
    int x = W - 25;
    int y = H - 8;
    // Draw larger block to indicate high level
    for (int px = x; px < x + 4; px++) {
      for (int py = y; py < y + 4; py++) {
        tv.set_pixel(px, py, 1);
      }
    }
  }
}

void initBricks() {
  bricksRemaining = 0;
  for (int row = 0; row < BRICK_ROWS; row++) {
    for (int col = 0; col < BRICK_COLS; col++) {
      bricks[row][col] = true;
      bricksRemaining++;
    }
  }
}

void initBreakout() {
  tv.fill(0);
  
  // Initialize ball position above paddle
  ballx = W / 2;
  bally = paddleY - 10;
  dx = 1;
  dy = 0;  // Will auto-serve
  
  // Initialize paddle
  paddleX = (W - paddleWidth) / 2;
  
  // Initialize bricks
  initBricks();
  
  // Reset game state
  lives = 3;
  level = 1;
  gameEnded = false;
  
  drawUI();
  
  // Draw initial ball
  tv.set_pixel(ballx, bally, 1);
  tv.set_pixel(ballx + 1, bally, 1);
  tv.set_pixel(ballx, bally + 1, 1);
  tv.set_pixel(ballx + 1, bally + 1, 1);
}

void levelComplete() {
  if (!attractMode) {
    level++;
    levelCompleteSound();
    tv.delay(2000);
  }
  
  // Reset for next level
  initBricks();
  ballx = paddleX + (paddleWidth / 2);
  bally = paddleY - 5;
  dx = random(0, 2) == 0 ? 1 : -1;
  dy = 0;
}

void gameOver() {
  tv.delay(1000);
  tv.fill(0);
  
  // Draw "GAME OVER"
  drawLargeGameOver();
  drawUI();
  gameEnded = true;
  tv.delay(3000);
}

void updateAIPaddle() {
  // Simple AI: try to keep paddle under the ball
  float targetX = ballx - (paddleWidth / 2);
  
  // Add some error for more realistic AI
  if (random(0, 100) < 5) {  // 5% chance of slight error
    targetX += random(-8, 9);
  }
  
  aiPaddleTarget = constrain(targetX, 0, W - paddleWidth);
  
  // Smooth movement
  float targetDistance = aiPaddleTarget - aiPaddleFloat;
  
  if (targetDistance > aiMaxSpeed) {
    targetDistance = aiMaxSpeed;
  } else if (targetDistance < -aiMaxSpeed) {
    targetDistance = -aiMaxSpeed;
  }
  
  // Apply momentum damping
  if ((aiMomentum > 0 && targetDistance < 0) || (aiMomentum < 0 && targetDistance > 0)) {
    targetDistance *= 0.7;
  }
  
  aiPaddleFloat += targetDistance;
  aiMomentum = targetDistance;
  
  paddleX = (byte)(aiPaddleFloat + 0.5);
  
  // Boundary check
  if (paddleX < 0) {
    paddleX = 0;
    aiPaddleFloat = 0.0;
    aiMomentum = 0.0;
  }
  if (paddleX > W - paddleWidth) {
    paddleX = W - paddleWidth;
    aiPaddleFloat = W - paddleWidth;
    aiMomentum = 0.0;
  }
}

// Sound effects
void hitSound() {
  if (!attractMode) playTone(523, 20);
}

void bounceSound() {
  if (!attractMode) playTone(261, 20);
}

void brickSound() {
  if (!attractMode) playTone(800, 30);
}

void missSound() {
  if (!attractMode) playTone(105, 500);
}

void levelCompleteSound() {
  if (!attractMode) {
    playTone(523, 100);
    tv.delay(50);
    playTone(659, 100);
    tv.delay(50);
    playTone(784, 100);
    tv.delay(50);
    playTone(1047, 200);
  }
}

void playTone(unsigned int frequency, unsigned long duration_ms) {
  tv.tone(frequency, duration_ms);
}

// All the intro screen and large letter drawing functions from your original code
void drawIntroScreen() {
  tv.fill(0);

  // Draw "SIDE"
  byte startX = 20; 
  byte lineSpacing = 18;  
  byte y1 = 10;
  drawLargeS(startX, y1);
  drawLargeI(startX + 20, y1);
  drawLargeD(startX + 40, y1);
  drawLargeE(startX + 60, y1);

  // Draw "BURN"
  byte y2 = y1 + lineSpacing;
  drawLargeB(startX, y2);
  drawLargeU(startX + 20, y2);
  drawLargeR(startX + 40, y2);
  drawLargeN(startX + 60, y2);

  // Draw "BREAKOUT"
  byte y3 = y2 + lineSpacing;
  drawLargeB(startX - 15, y3);
  drawLargeR(startX - 5, y3);
  drawLargeE(startX + 5, y3);
  drawLargeA(startX + 15, y3);
  drawLargeK(startX + 25, y3);
  drawLargeO(startX + 35, y3);
  drawLargeU(startX + 45, y3);
  drawLargeT(startX + 55, y3);

  tv.delay(3000);
}

// Large letter drawing functions (keeping your existing ones and adding new ones)
void drawLargeS(byte x, byte y) {
  tv.draw_line(x+1, y, x+6, y, 1);
  tv.draw_line(x, y+1, x, y+5, 1);
  tv.draw_line(x+1, y+6, x+6, y+6, 1);
  tv.draw_line(x+7, y+7, x+7, y+10, 1);
  tv.draw_line(x+1, y+11, x+6, y+11, 1);
}

void drawLargeI(byte x, byte y) {
  tv.draw_line(x, y, x+6, y, 1);
  tv.draw_line(x+3, y, x+3, y+11, 1);
  tv.draw_line(x, y+11, x+6, y+11, 1);
}

void drawLargeD(byte x, byte y) {
  tv.draw_line(x, y, x, y+11, 1);
  tv.draw_line(x+1, y, x+5, y, 1);
  tv.draw_line(x+6, y+1, x+6, y+10, 1);
  tv.draw_line(x+1, y+11, x+5, y+11, 1);
}

void drawLargeE(byte x, byte y) {
  tv.draw_line(x, y, x, y+11, 1);
  tv.draw_line(x+1, y, x+6, y, 1);
  tv.draw_line(x+1, y+6, x+5, y+6, 1);
  tv.draw_line(x+1, y+11, x+6, y+11, 1);
}

void drawLargeB(byte x, byte y) {
  tv.draw_line(x, y, x, y+11, 1);
  tv.draw_line(x+1, y, x+5, y, 1);
  tv.draw_line(x+6, y+1, x+6, y+5, 1);
  tv.draw_line(x+1, y+6, x+5, y+6, 1);
  tv.draw_line(x+6, y+7, x+6, y+10, 1);
  tv.draw_line(x+1, y+11, x+5, y+11, 1);
}

void drawLargeU(byte x, byte y) {
  tv.draw_line(x, y, x, y+10, 1);
  tv.draw_line(x+7, y, x+7, y+10, 1);
  tv.draw_line(x+1, y+11, x+6, y+11, 1);
}

void drawLargeR(byte x, byte y) {
  tv.draw_line(x, y, x, y+11, 1);
  tv.draw_line(x+1, y, x+5, y, 1);
  tv.draw_line(x+6, y+1, x+6, y+5, 1);
  tv.draw_line(x+1, y+6, x+5, y+6, 1);
  tv.draw_line(x+3, y+7, x+4, y+8, 1);
  tv.draw_line(x+5, y+9, x+6, y+11, 1);
}

void drawLargeN(byte x, byte y) {
  tv.draw_line(x, y, x, y+11, 1);
  tv.draw_line(x+7, y, x+7, y+11, 1);
  tv.draw_line(x, y, x+7, y+11, 1);
}

void drawLargeA(byte x, byte y) {
  tv.draw_line(x+1, y, x+5, y, 1);
  tv.draw_line(x+1, y, x+1, y+11, 1);
  tv.draw_line(x+8, y, x+8, y+11, 1);
  tv.draw_line(x+1, y+7, x+5, y+7, 1);
}

void drawLargeK(byte x, byte y) {
  tv.draw_line(x, y, x, y+11, 1);
  tv.draw_line(x+6, y, x+1, y+5, 1);
  tv.draw_line(x+1, y+6, x+6, y+11, 1);
}

void drawLargeO(byte x, byte y) {
  tv.draw_line(x+1, y, x+6, y, 1);
  tv.draw_line(x, y+1, x, y+10, 1);
  tv.draw_line(x+7, y+1, x+7, y+10, 1);
  tv.draw_line(x+1, y+11, x+6, y+11, 1);
}

void drawLargeT(byte x, byte y) {
  tv.draw_line(x, y, x+6, y, 1);
  tv.draw_line(x+3, y, x+3, y+11, 1);
}

void drawLargeGameOver() {
  byte gameX = 30;
  byte gameY = 35;
  byte overX = 30;
  byte overY = 55;
  byte letterSpacing = 20;
  
  // Draw "GAME" 
  drawLargeG(gameX, gameY);
  drawLargeA(gameX + letterSpacing, gameY);
  drawLargeM(gameX + (letterSpacing * 2), gameY);
  drawLargeE(gameX + (letterSpacing * 3), gameY);
  
  // Draw "OVER"
  drawLargeO(overX, overY);
  drawLargeV(overX + letterSpacing, overY);
  drawLargeE(overX + (letterSpacing * 2), overY);
  drawLargeR(overX + (letterSpacing * 3), overY);
}

void drawLargeG(byte x, byte y) {
  tv.draw_line(x+1, y, x+6, y, 1);
  tv.draw_line(x, y+1, x, y+10, 1);
  tv.draw_line(x+1, y+11, x+6, y+11, 1);
  tv.draw_line(x+7, y+7, x+7, y+10, 1);
  tv.draw_line(x+4, y+6, x+7, y+6, 1);
}

void drawLargeM(byte x, byte y) {
  tv.draw_line(x, y, x, y+11, 1);
  tv.draw_line(x+8, y, x+8, y+11, 1);
  tv.draw_line(x, y, x+4, y+4, 1);
  tv.draw_line(x+5, y, x+4, y+4, 1);
}

void drawLargeV(byte x, byte y) {
  tv.draw_line(x, y, x+1, y+8, 1);
  tv.draw_line(x+6, y, x+5, y+8, 1);
  tv.draw_line(x+2, y+9, x+2, y+9, 1);
  tv.draw_line(x+5, y+9, x+5, y+9, 1);
  tv.draw_line(x+3, y+10, x+4, y+10, 1);
  tv.set_pixel(x+3, y+11, 1);
  tv.set_pixel(x+4, y+11, 1);
}