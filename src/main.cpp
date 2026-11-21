#include <Arduino.h>

#include "SSD1306.h"
#include "qrcode.h"


// Set where to find the board
#ifdef DIYMORE_LORA_V4
SSD1306 display(0x3c, 17, 18);
#else
SSD1306  display(0x3c, 21, 22); // Specify pins display is connected to, might have to change address
#endif
QRcode qrcode (&display);

// Set display stuff
const int len = 128; // 128 native pixels / 4 pixels per cell = 32 virtual cells length     32
const int ht = 64; // 64 native pixels / 4 pixels per cell = 16 virtual cells height        16
const int pixelSize = 1; // Set how many native pixels = pixels per cell length or width    4 2
const bool flipScreen = true; // If you need to flip the screen                             false


// Counter and seed value
int seedValue = 0;
int counter = 0;
int maxCycles = 500; // 500 cycles per game * 0.1 sec per cycle = 50 second games

// Initialize the board
int board[len][ht];
int newBoard[len][ht];

// Function prototypes
int countNeighbors(int, int);
void drawBoard();
void randomBoard();
void makeNewBoard();
// Runs when powering on
void setup(){
  // Init the display
#ifdef DIYMORE_LORA_V4
  pinMode(36, OUTPUT);
  digitalWrite(36, LOW); // Heltec-compatible V4 Vext supply
  delay(50);
  pinMode(21, OUTPUT);
  digitalWrite(21, LOW);
  delay(10);
  digitalWrite(21, HIGH);
  delay(50);
#endif
  qrcode.init();
  if(flipScreen == true){
    display.flipScreenVertically();
  }

  // Display the title
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.drawString(40, 15, F("Conway\'s"));
  display.drawString(32, 30, F("Game of Life"));
  display.drawString(0, 51, "Starting game " + String(seedValue + 1) + "...");
  display.display();
  delay(2000);
  qrcode.create("Starting game " + String(seedValue + 1) + "...");
  delay(2000);
  display.clear();

  // Seed the board
  randomBoard();
}

// Main loop
void loop() {
  // Catch for number of cycles per game
  if(counter == maxCycles){
    randomSeed(seedValue);
    randomBoard();
    seedValue++;
    counter = 0;
    display.clear();
    display.drawString(0, 51, "Starting game " + String(seedValue + 1) + "...");
    display.display();
    delay(2000);
    display.clear();
    
    // create qrcode
    qrcode.create("Starting game " + String(seedValue + 1) + "...");
    delay(2000);
  }

  // Draw the board, increment cycle counter
  drawBoard();
  makeNewBoard();
  counter++;

  // Do a 0.1 second delay
  delay(100);
}

void drawBoard(){
  display.setColor(WHITE);
  display.clear();
  for(int x=0; x<len; x++){
    for(int y=0; y<ht; y++){
      if(board[x][y] == 1){
        display.fillRect(x*pixelSize, y*pixelSize, pixelSize, pixelSize);
      }
    }
  }
  display.display();
}

int countNeighbors(int x, int y){
  int total = 0;
  for (int nx = max(0, x - 1); nx <= min(len - 1, x + 1); ++nx) {
    for (int ny = max(0, y - 1); ny <= min(ht - 1, y + 1); ++ny) {
      total += board[nx][ny];
    }
  }
  return total - board[x][y];
}

void makeNewBoard(){
  for(int x=0; x<len; x++){
    for(int y=0; y<ht; y++){
      int neighbors = countNeighbors(x, y);
      if(board[x][y] == 1 && (neighbors == 2 || neighbors == 3)){
        newBoard[x][y] = 1;
      }
      else if(board[x][y] == 1){
        newBoard[x][y] = 0;
      }
      if(board[x][y] == 0 && (neighbors == 3)){
        newBoard[x][y] = 1;
      }
      else if (board[x][y] == 0){
        newBoard[x][y] = 0;
      }
    }
  }
  for(int x=0; x<len; x++){
    for(int y=0; y<ht; y++){
      board[x][y] = newBoard[x][y];
    }
  }
}

void randomBoard(){
  for(int x=0; x<len; x++){
    for(int y=0; y<ht; y++){
      board[x][y] = random(0,2);
    }
  }
}
