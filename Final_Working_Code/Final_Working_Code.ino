#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include "Waveshare_LCD1602_RGB.h"
#include <DFRobotDFPlayerMini.h>

// ================= DFMini MP3 Player ================= //
DFRobotDFPlayerMini mp3;

// ================= RFID ================= //
#define RFID_SS  53
#define RFID_RST 49
MFRC522 rfid(RFID_SS, RFID_RST);


byte allowedUID[] = {0xB5, 0x7A, 0xFE, 0x29};

// ================= LCD ================= //
Waveshare_LCD1602_RGB lcd(16, 2);

// ================= BUTTONS ================= //
#define BTN_COUNT  2  
#define BTN_NEXT   3  
#define BTN_RESET  4   

bool lastCountState = HIGH;
bool lastNextState  = HIGH;
bool lastResetState = HIGH;

// ================= SOLENOIDS ================= //
#define SOL1 9
#define SOL2 8

// ================= GAME ================= //
enum State { WAIT_CARD, GAME, WIN };
State state = WAIT_CARD;
unsigned long now;
unsigned long btnTimer  = 0;
unsigned long audioTimer = 0;
unsigned long debounceDelay = 200;
bool isPlayingAudio = false;
int counter = 0;
int qIndex  = 0;
int currentTrack = 0;
bool lcdWorking = false;

// Denial display
bool showingDeny = false;
unsigned long denyTimer = 0;

int totalQuestions = 10;  

// Win state timers
unsigned long winStartTime = 0;
const unsigned long winTotalDuration = 6000;   
const unsigned long solenoidDuration = 4000;   


unsigned long lastWelcomePlayTime = 0;
const unsigned long welcomeCooldown = 5000;    
const unsigned long welcomeInterval = 10000;   

// ================= FRUITS ================= //
struct Fruit { 
    const char* name; 
    int qty; 
    int audioFile;  // Audio file number
};


Fruit fruits[] = {
    {"Apples", 1, 5},           // File 0005.mp3 = 1 apple
    {"Pineapples", 2, 6},       // File 0006.mp3 = 2 pineapples
    {"Oranges", 3, 7},          // File 0007.mp3 = 3 oranges
    {"Mango pieces", 4, 8},     // File 0008.mp3 = 4 mango pieces
    {"Bananas", 5, 9},          // File 0009.mp3 = 5 bananas
    {"Watermelon slices", 6, 10}, // File 0010.mp3 = 6 watermelon slices
    {"Kiwi slices", 7, 11},     // File 0011.mp3 = 7 kiwi slices
    {"Plums", 8, 12},           // File 0012.mp3 = 8 plums
    {"Strawberries", 9, 13},    // File 0013.mp3 = 9 strawberries
    {"Grapes", 10, 14}          // File 0014.mp3 = 10 grapes
};

int order[10];

// ================= AUDIO ================= // 
unsigned long lastPlayTime = 0;  

void playAudio(int track) {
    
    if (track == currentTrack && (now - lastPlayTime) < 500) {
        Serial.println("[AUDIO] Skipped duplicate request");
        return;
    }
    lastPlayTime = now;

    if (track >= 1 && track <= 16) {
        Serial.print("[AUDIO] Playing: ");
        if(track < 10) Serial.print("0");
        if(track < 100) Serial.print("0");
        Serial.print(track);
        Serial.print(".mp3");
        
        
        mp3.stop();
        delay(50);
        

        mp3.play(track);
        isPlayingAudio = true;
        audioTimer = now;
        currentTrack = track;
        
        
        switch(track) {
            case 1: Serial.println(" (tap the card)"); break;
            case 2: Serial.println(" (access granted)"); break;
            case 3: Serial.println(" (access denied)"); break;
            case 4: Serial.println(" (well done)"); break;
            case 5: Serial.println(" (apples - answer: 1)"); break;
            case 6: Serial.println(" (pineapples - answer: 2)"); break;
            case 7: Serial.println(" (oranges - answer: 3)"); break;
            case 8: Serial.println(" (mango - answer: 4)"); break;
            case 9: Serial.println(" (bananas - answer: 5)"); break;
            case 10: Serial.println(" (watermelon - answer: 6)"); break;
            case 11: Serial.println(" (kiwi - answer: 7)"); break;
            case 12: Serial.println(" (plum - answer: 8)"); break;
            case 13: Serial.println(" (strawberries - answer: 9)"); break;
            case 14: Serial.println(" (grapes - answer: 10)"); break;
            case 15: Serial.println(" (correct tone)"); break;
            case 16: Serial.println(" (incorrect tone)"); break;
            default: Serial.println(); break;
        }
    }
}

void stopAudio() {
    mp3.stop();
    isPlayingAudio = false;
    Serial.println("[AUDIO] Stopped");
}

// audio completion check //
bool checkAudioFinished() {
    if (!isPlayingAudio) return true;
    
   
    unsigned long estimatedDuration;
    
    if (currentTrack == 1) estimatedDuration = 3000;      
    else if (currentTrack == 2) estimatedDuration = 2000; 
    else if (currentTrack == 3) estimatedDuration = 2000; 
    else if (currentTrack == 4) estimatedDuration = 5000; 
    else if (currentTrack >= 5 && currentTrack <= 14) estimatedDuration = 4000; 
    else if (currentTrack == 15 || currentTrack == 16) estimatedDuration = 1000; 
    else estimatedDuration = 4000; 
    
    
    if (now - audioTimer > estimatedDuration + 500) {
        isPlayingAudio = false;
        return true;
    }
    
    return false;
}

void playWelcomeMessage() {
    
    if (!isPlayingAudio && (now - lastWelcomePlayTime > welcomeCooldown)) {
        lastWelcomePlayTime = now;
        playAudio(1); // "tap the card"
    }
}

void playAccessGranted() {
    playAudio(2); // "access granted"
}

void playAccessDenied() {
    playAudio(3); // "access denied"
}

void playWinMessage() {
    playAudio(4); // "well done"
}

void playCorrectTone() {
    playAudio(15); // "correct tone"
}

void playIncorrectTone() {
    playAudio(16); // "incorrect tone"
}

void playQuestionAudio() {
    int idx = order[qIndex];
    int track = fruits[idx].audioFile;
    playAudio(track);
}

// ================= LCD ================= //
void showScanMessage() {
    if(!lcdWorking) return;
    lcd.setRGB(0, 0, 255); 
    lcdClear();
    lcd.setCursor(0, 0);
    lcd.send_string("Scan a card");
}

void showAccessGrantedScreen() {
    if(!lcdWorking) return;
    lcd.setRGB(0, 255, 0); 
    lcdClear();
    lcd.setCursor(0, 0);
    lcd.send_string("Access Granted!");
    lcd.setCursor(0, 1);
    lcd.send_string("Good job");
    delay(2000); 
}

void showDenyMessage() {
    if(!lcdWorking) return;
    lcd.setRGB(255, 0, 0); 
    lcdClear();
    lcd.setCursor(0, 0);
    lcd.send_string("Access Denied");
    lcd.setCursor(0, 1);
    lcd.send_string("Wrong Card");
}

void shuffleQ() {
    randomSeed(analogRead(0));
    for(int i = 0; i < 10; i++) order[i] = i;
    for(int i = 9; i > 0; i--) {
        int j = random(i + 1);
        int t = order[i]; 
        order[i] = order[j]; 
        order[j] = t;
    }
}

void lcdClear() {
    if(!lcdWorking) return;
    lcd.setCursor(0, 0); 
    lcd.send_string("                ");
    lcd.setCursor(0, 1); 
    lcd.send_string("                ");
}

void lcdCounter() {
    if(!lcdWorking) return;
    char buffer[17];
    snprintf(buffer, sizeof(buffer), "Count: %2d", counter);
    lcd.setCursor(0, 0);
    lcd.send_string(buffer);
}

void showQuestion() {
    int idx = order[qIndex];
    
    // Display on LCD
    if(lcdWorking) {
        lcdClear();
        lcd.setCursor(0, 0);
        lcd.send_string("How many?");
        lcd.setCursor(0, 1);
        lcd.send_string(fruits[idx].name);
    }
    
    
    Serial.println("");
    Serial.print("=== Question ");
    Serial.print(qIndex + 1);
    Serial.print("/");
    Serial.print(totalQuestions);
    Serial.print(" ===");
    
    int audioFile = fruits[idx].audioFile;
    Serial.print(" File ");
    if(audioFile < 10) Serial.print("0");
    Serial.print(audioFile);
    Serial.print(".mp3: How many ");
    Serial.print(fruits[idx].name);
    Serial.print("? (Correct: ");
    Serial.print(fruits[idx].qty);
    Serial.println(")");
    
    /
    delay(300);
    playQuestionAudio();
}

// ============== LOADING SCREEN ============== //
void showLoadingScreen() {
    if(!lcdWorking) return;
    lcd.setRGB(0, 0, 255); 
    lcd.setCursor(0, 0);
    lcd.send_string("Loading...");
    
    
    lcd.setCursor(0, 1);
    lcd.send_string("[");               
    for (int i = 1; i <= 14; i++) {
        lcd.setCursor(i, 1);
        lcd.send_string(" ");            
    }
    lcd.setCursor(15, 1);
    lcd.send_string("]");                
    
    
    for (int i = 1; i <= 14; i++) {
        lcd.setCursor(i, 1);
        lcd.send_string("#");            
        delay(200);                      
    }
    lcdClear();
}

// ============== DIFFICULTY SELECTION ============== //
void selectDifficultyAfterCard() {
    if(!lcdWorking) return;
    lcd.setRGB(255, 255, 0); // Yellow
    lcd.setCursor(0, 0);
    lcd.send_string("Easy  Med  Hard");
    lcd.setCursor(0, 1);
    lcd.send_string("Count Next Reset");
    Serial.println("Choose difficulty: Press Count (Easy), Next (Medium), Reset (Hard)");

    while (true) {
        if (digitalRead(BTN_COUNT) == LOW) {
            delay(50);
            if (digitalRead(BTN_COUNT) == LOW) {
                while (digitalRead(BTN_COUNT) == LOW) delay(10);
                totalQuestions = 3;
                Serial.println("Easy mode selected: 3 questions");
                break;
            }
        }
        if (digitalRead(BTN_NEXT) == LOW) {
            delay(50);
            if (digitalRead(BTN_NEXT) == LOW) {
                while (digitalRead(BTN_NEXT) == LOW) delay(10);
                totalQuestions = 7;
                Serial.println("Medium mode selected: 7 questions");
                break;
            }
        }
        if (digitalRead(BTN_RESET) == LOW) {
            delay(50);
            if (digitalRead(BTN_RESET) == LOW) {
                while (digitalRead(BTN_RESET) == LOW) delay(10);
                totalQuestions = 10;
                Serial.println("Hard mode selected: 10 questions");
                break;
            }
        }
        delay(50);
    }
    
    
    lcdClear();
    lcdCounter();  
}

// ================= GAME WON EFFECT ================= //
void setRainbowColor(unsigned long elapsed, unsigned long totalDuration) {
    
    float hue = (float)elapsed / totalDuration * 360.0;
   
    int r, g, b;
    int segment = (int)hue / 60;
    float frac = (hue / 60.0) - segment;
    
    switch(segment % 6) {
        case 0: r = 255; g = (int)(255 * frac); b = 0; break;
        case 1: r = (int)(255 * (1 - frac)); g = 255; b = 0; break;
        case 2: r = 0; g = 255; b = (int)(255 * frac); break;
        case 3: r = 0; g = (int)(255 * (1 - frac)); b = 255; break;
        case 4: r = (int)(255 * frac); g = 0; b = 255; break;
        default: r = 255; g = 0; b = (int)(255 * (1 - frac)); break;
    }
    
    lcd.setRGB(r, g, b);
}




// ================= SETUP =================
void setup() {
    Serial.begin(9600);
    delay(1000);
    Serial.println("========================================");
    Serial.println("FRUIT COUNTING GAME");
    Serial.println("========================================");
    
    randomSeed(analogRead(0));
    
    // Buttons
    pinMode(BTN_COUNT, INPUT_PULLUP);
    pinMode(BTN_NEXT, INPUT_PULLUP);
    pinMode(BTN_RESET, INPUT_PULLUP);
    Serial.println("Buttons ready");
    
    // Solenoids
    pinMode(SOL1, OUTPUT);
    pinMode(SOL2, OUTPUT);
    digitalWrite(SOL1, LOW);
    digitalWrite(SOL2, LOW);
    Serial.println("Solenoids ready");
    
    // I2C and LCD
    Serial.println("Initializing LCD...");
    Wire.begin();
    Wire.setClock(100000L);
    delay(200);
    
    lcd.init();
    delay(300);
    lcdWorking = true;
    
   
    showLoadingScreen();
    
    // RFID
    Serial.println("Initializing RFID...");
    SPI.begin();
    delay(50);
    rfid.PCD_Init();
    delay(100);
    
    byte version = rfid.PCD_ReadRegister(rfid.VersionReg);
    if (version == 0x00 || version == 0xFF) {
        Serial.println("WARNING: RFID communication failed");
    } else {
        Serial.print("RFID ready - Version: 0x");
        Serial.println(version, HEX);
    }
    
    // DFMini MP3 Player
    Serial.println("Initializing DFMini MP3...");
    Serial1.begin(9600);
    delay(300);
    
    if (!mp3.begin(Serial1)) {
        Serial.println("ERROR: Unable to initialize DFMini player!");
    } else {
        Serial.println("DFMini MP3 player ready");
        mp3.volume(30);
        mp3.EQ(DFPLAYER_EQ_NORMAL);
        mp3.outputDevice(DFPLAYER_DEVICE_SD);
    }
    
    
    shuffleQ();
    state = WAIT_CARD;
    
    
    showScanMessage();
    playWelcomeMessage();  
    
    Serial.println("========================================");
    Serial.println("GAME READY - Tap card to start");
    Serial.println("========================================");
}




// ================= LOOP ================= //
void loop() {
    now = millis();
    
    
    if (isPlayingAudio && checkAudioFinished()) {
        isPlayingAudio = false;
    }
    
    
    if (showingDeny && now >= denyTimer) {
        showingDeny = false;
        showScanMessage();
    }
    

    // -------------------- WAIT CARD -------------------- //
    if(state == WAIT_CARD) {
        
        if(!showingDeny && now - lastWelcomePlayTime > welcomeInterval && !isPlayingAudio) {
            playWelcomeMessage();
        }
        
        if (!showingDeny && rfid.PICC_IsNewCardPresent()) {
            if (rfid.PICC_ReadCardSerial()) {
                Serial.print("[RFID] CARD DETECTED: ");
                for(byte i = 0; i < rfid.uid.size; i++) {
                    if(rfid.uid.uidByte[i] < 0x10) Serial.print("0");
                    Serial.print(rfid.uid.uidByte[i], HEX);
                    if(i < rfid.uid.size - 1) Serial.print(" ");
                }
                Serial.println();
                
                bool cardAllowed = true;
                if (rfid.uid.size != 4) {
                    cardAllowed = false;
                } else {
                    for (byte i = 0; i < 4; i++) {
                        if (rfid.uid.uidByte[i] != allowedUID[i]) {
                            cardAllowed = false;
                            break;
                        }
                    }
                }
                
                rfid.PICC_HaltA();
                rfid.PCD_StopCrypto1();
                
                if (cardAllowed) {
                    Serial.println("[RFID] ALLOWED CARD - Starting game");
                    playAccessGranted();
                    
                    showAccessGrantedScreen();
                    
                    delay(500);
                    
                    selectDifficultyAfterCard();
                    
                    state = GAME;
                    counter = 0;
                    qIndex = 0;
                    
                    if(lcdWorking) {
                        lcd.setRGB(0, 0, 255); 
                        lcdCounter();
                    }
                    
                    Serial.println("========================================");
                    Serial.println("[GAME STARTED]");
                    Serial.print("You need ");
                    Serial.print(totalQuestions);
                    Serial.println(" correct answers.");
                    Serial.println("========================================");
                    
                    delay(1000);  
                    showQuestion();
                } else {
                    Serial.println("[RFID] UNAUTHORIZED CARD - Access denied");
                    playAccessDenied();
                    
                    showDenyMessage();
                    showingDeny = true;
                    denyTimer = now + 2000;
                }
            }
        }
    }
    


    // -------------------- GAME -------------------- //
    else if(state == GAME) {
        if(now - btnTimer > debounceDelay) {
            int countState = digitalRead(BTN_COUNT);
            int nextState  = digitalRead(BTN_NEXT);
            int resetState = digitalRead(BTN_RESET);
            
            // Count button
            if (countState == LOW && lastCountState == HIGH) {
                btnTimer = now;
                counter++;
                Serial.print("[COUNT] -> "); 
                Serial.println(counter);
                lcdCounter();
            }
            // Reset button
            else if (resetState == LOW && lastResetState == HIGH) {
                btnTimer = now;
                counter = 0;
                Serial.println("[RESET] Counter = 0");
                lcdCounter();
            }
            // Next button 
            else if (nextState == LOW && lastNextState == HIGH) {
                btnTimer = now;
                Serial.print("[ANSWER] "); 
                Serial.print(counter);
                
                int correct = fruits[order[qIndex]].qty;
                int audioFile = fruits[order[qIndex]].audioFile;
                
                if(counter == correct) {
                    Serial.println(" - CORRECT! ✓");
                    Serial.print("File ");
                    if(audioFile < 10) Serial.print("0");
                    Serial.print(audioFile);
                    Serial.print(".mp3 = ");
                    Serial.print(correct);
                    Serial.print(" (");
                    Serial.print(audioFile);
                    Serial.print(" - 4)");
                    
                    playCorrectTone();
                    
                    qIndex++;
                    counter = 0;
                    lcdCounter();
                    
                    if(qIndex >= totalQuestions) {
                        state = WIN;
                        winStartTime = now;   
                        
                        
                        digitalWrite(SOL1, HIGH);
                        digitalWrite(SOL2, HIGH);
                        
                        Serial.println("");
                        Serial.println("========================================");
                        Serial.println("*** YOU WIN! ***");
                        Serial.println("CONGRATULATIONS!");
                        Serial.println("========================================");
                        
                        
                        playWinMessage();
                        
                        if(lcdWorking) {
                            lcdClear();
                            lcd.setCursor(0, 0);
                            lcd.send_string("YOU WIN!");
                            lcd.setCursor(0, 1);
                            lcd.send_string("CONGRATULATIONS!");
                        }
                    } else {
                        Serial.println("");
                        Serial.print("Next question: ");
                        Serial.println(qIndex + 1);
                        delay(1500);
                        showQuestion();
                    }
                } else {
                    Serial.print(" - WRONG! (Correct: ");
                    Serial.print(correct);
                    Serial.print(" = ");
                    Serial.print(audioFile);
                    Serial.print(" - 4)");
                    Serial.println("");
                    
                    playIncorrectTone();
                    
                    counter = 0;
                    lcdCounter();
                    Serial.println("Try again...");
                    delay(1500);
                    showQuestion();
                }
            }
            
            lastCountState = countState;
            lastNextState  = nextState;
            lastResetState = resetState;
        }
    }
    
    // -------------------- WIN -------------------- // 
    else if(state == WIN) {
        
        if (now - winStartTime >= solenoidDuration) {
            digitalWrite(SOL1, LOW);
            digitalWrite(SOL2, LOW);
        }
        
        
        unsigned long elapsed = now - winStartTime;
        if (elapsed <= winTotalDuration) {
            setRainbowColor(elapsed, winTotalDuration);
        }
        
        
        if (elapsed >= winTotalDuration) {
            state = WAIT_CARD;
            
            if(lcdWorking) {
                showScanMessage(); 
            }
            
            shuffleQ();
            Serial.println("");
            Serial.println("========================================");
            Serial.println("Game reset - Scan card to play again");
            Serial.println("========================================");
            
            delay(1000);
            playWelcomeMessage();   
        }
    }
}