#include "display.h"
#include "logger.h"
#include "memory_manager.h"
#include <SPI.h>
#include <U8g2lib.h>
#include "modules/module.h"
#include "terminal.h"

extern SemaphoreHandle_t spiMutex;

// Change from software SPI to hardware SPI
U8G2_SSD1309_128X64_NONAME0_F_4W_HW_SPI u8g2(U8G2_R0, DISPLAY_CS_PIN, DISPLAY_DC_PIN, DISPLAY_RES_PIN);

Display::State Display::currentState = Display::State::TERMINAL;

// Initialize animation variables
int Display::loadingAngle = 0;
int Display::loadingDots = 0;
int Display::animationCounter = 0;
int Display::bootStep = 0;
int Display::stepProgress = 0;
bool Display::stepCompleted = false;

void Display::Setup() {
    LOG_INFO("Display setup");
    u8g2.begin();
}

void Display::Run() {
    Terminal::Setup();

    // Skip MemoryManager for Display - it's causing system hangs
    // Display will work directly with available heap memory
    bool memoryReserved = true; // Always assume we have memory for basic display operations
    
    LOG_INFO("Display: Running in direct memory mode (bypassing MemoryManager)");

    // Reduced update frequency for degraded mode
    const int normalUpdateMs = 25;
    const int degradedUpdateMs = 250; // Much slower when no memory reserved (10x slower)
    
    while (true) {
        // Simple memory check without MemoryManager
        static unsigned long lastMemoryCheck = 0;
        if (millis() - lastMemoryCheck > 10000) { // Check every 10 seconds
            size_t freeHeap = ESP.getFreeHeap();
            if (freeHeap < 50000) { // Less than 50KB
                LOG_WARNINGF("Display: Low memory detected: %zu bytes", freeHeap);
            }
            lastMemoryCheck = millis();
        }
        
        // Skip display update if memory is extremely low
        if (ESP.getFreeHeap() < 10000) { // Less than 10KB
            vTaskDelay(pdMS_TO_TICKS(degradedUpdateMs));
            continue;
        }

        if (xSemaphoreTake(spiMutex, portMAX_DELAY) == pdTRUE) {
            // Only draw if we have memory reserved OR if we're in emergency mode
            if (memoryReserved) {
                switch (currentState) {
                    case State::TERMINAL:
                        drawTerminal();
                        break;
                    case State::DASHBOARD:
                        drawDashboard();
                        break;
                }
            }

            xSemaphoreGive(spiMutex);
        }
        
        // Use different update frequencies based on memory availability
        vTaskDelay(pdMS_TO_TICKS(memoryReserved ? normalUpdateMs : degradedUpdateMs));
    }
    
    // No memory cleanup needed in direct mode
}

void Display::drawTerminal() {
    const auto& console_lines = Terminal::GetLines();

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x7_tf);
    int yPos = 10;
    char progress_str[10];

    int linesToShow = std::min(6, (int)console_lines.size());
    int startIndex = std::max(0, (int)console_lines.size() - 6);

    int group_width[linesToShow];

    for (int i = 0; i < linesToShow; i++) {
        int lineIndex = startIndex + i;
        char group_with_brackets[10];
        snprintf(group_with_brackets, sizeof(group_with_brackets), "[%s]", console_lines[lineIndex].group.c_str());
        u8g2.drawStr(0, yPos, group_with_brackets);
        group_width[i] = u8g2.getStrWidth(group_with_brackets);
        yPos += 10;
    }

    yPos = 10;
    int xPos = 20;
    u8g2.setFont(u8g2_font_4x6_tf);

    // Get mutable reference to console_lines for animation
    auto& mutable_console_lines = const_cast<std::vector<Terminal::ConsoleLine>&>(console_lines);

    for (int i = 0; i < linesToShow; i++) {
        int lineIndex = startIndex + i;
        xPos = group_width[i] + 1;
        int charCount = 24 - mutable_console_lines[lineIndex].group.length();
        if (mutable_console_lines[lineIndex].status.length() == 0) {
            charCount -= 1;
        }

        char description[30];
        int startPos = int(mutable_console_lines[lineIndex].offsetX);
        for (int j = 0; j < charCount; j++) {
            if (startPos + j < 0) {
                description[j] = ' ';  // Show spaces during pause
            } else {
                description[j] = mutable_console_lines[lineIndex].description[startPos + j];
                if (description[j] == '\0') {
                    break;
                }
            }
        }
        description[charCount] = '\0';

        int descriptionLength = mutable_console_lines[lineIndex].description.length();
        if (descriptionLength > charCount) {
            // Smoother animation with smaller increments
            mutable_console_lines[lineIndex].offsetX += 0.12;

            // Calculate proper loop point for seamless scrolling
            int maxOffset = descriptionLength - charCount;
            if (mutable_console_lines[lineIndex].offsetX > maxOffset + 3) {  // Add 3 char pause at end
                mutable_console_lines[lineIndex].offsetX = -3;               // Start 3 chars before beginning for pause
            }
        }

        u8g2.drawStr(xPos, yPos, description);
        if (mutable_console_lines[lineIndex].status.length() > 0) {
            char status_with_brackets[10];
            snprintf(status_with_brackets, sizeof(status_with_brackets), "[%s]",
                     mutable_console_lines[lineIndex].status.c_str());
            drawRightAlignedText(status_with_brackets, yPos);
        } else {
            switch (loadingDots) {
                case 0:
                    strcpy(progress_str, "[.  ]");
                    break;
                case 1:
                    strcpy(progress_str, "[.. ]");
                    break;
                case 2:
                    strcpy(progress_str, "[...]");
                    break;
                case 3:
                    strcpy(progress_str, "[   ]");
                    break;
            }
            drawRightAlignedText(progress_str, yPos);
        }
        yPos += 10;
    }

    u8g2.sendBuffer();
    animationCounter++;

    if (animationCounter % 15 == 0) {
        loadingDots = (loadingDots + 1) % 4;
    }
}

void Display::drawDashboard() {
    u8g2.clearBuffer();

    // First pass: Draw all non-overlay modules
    for (int i = 0; i < active_modules.size(); i++) {
        modules::IModule* module = active_modules[i];
        if (!module->IsOverlay()) {
            module->Draw();
        }
    }
    
    // Second pass: Draw overlay modules on top
    for (int i = 0; i < active_modules.size(); i++) {
        modules::IModule* module = active_modules[i];
        if (module->IsOverlay()) {
            module->Draw();
        }
    }

    u8g2.sendBuffer();
}

void Display::drawRightAlignedText(const char* text, int y) {
    int textWidth = u8g2.getStrWidth(text);
    int x = 128 - textWidth;
    u8g2.drawStr(x, y, text);
}

void Display::SetState(State state) { currentState = state; }