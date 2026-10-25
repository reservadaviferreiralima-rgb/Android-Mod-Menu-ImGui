#include <imgui.h>
#include <vector>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <string>

// ==========================================
// ESTRUTURAS E DEFINIÇÕES
// ==========================================
struct Vector3 { float x, y, z; };
struct Vector2 { float x, y; };

// Coloque os offsets do SEU dump aqui
#define OFFSET_CAMERA_MAIN 0x0
#define OFFSET_CAMERA_MATRIX 0x0
#define OFFSET_PLAYER_LIST 0x0
#define OFFSET_LIST_COUNT 0x0
#define OFFSET_LIST_ITEMS 0x0
#define OFFSET_IS_LOCAL_PLAYER 0x0
#define OFFSET_IS_DEAD 0x0
#define OFFSET_HEAD_POS 0x0
#define OFFSET_FEET_POS 0x0
#define OFFSET_HEALTH 0x0
#define OFFSET_BONE_ARRAY 0x0

// ID dos ossos (geralmente mudam, cheque no dump)
#define BONE_HEAD 0
#define BONE_NECK 1
#define BONE_PELVIS 2

// ==========================================
// MATEMÁTICA: WORLD TO SCREEN
// ==========================================
bool WorldToScreen(Vector3 pos, Vector2& screen, float* matrix, int windowWidth, int windowHeight) {
    Vector3 clipCoords;
    clipCoords.x = pos.x * matrix[0] + pos.y * matrix[4] + pos.z * matrix[8] + matrix[12];
    clipCoords.y = pos.x * matrix[1] + pos.y * matrix[5] + pos.z * matrix[9] + matrix[13];
    clipCoords.z = pos.x * matrix[2] + pos.y * matrix[6] + pos.z * matrix[10] + matrix[14];
    float w = pos.x * matrix[3] + pos.y * matrix[7] + pos.z * matrix[11] + matrix[15];

    if (w < 0.01f) return false;

    Vector3 ndc;
    ndc.x = clipCoords.x / w;
    ndc.y = clipCoords.y / w;

    screen.x = (windowWidth / 2.0f) * (ndc.x + 1.0f);
    screen.y = (windowHeight / 2.0f) * (1.0f - ndc.y);

    return true;
}

// ==========================================
// LOOP PRINCIPAL DO ESP
// ==========================================
void DrawESP(uintptr_t base) {
    if (base == 0) return;

    int screenW = ImGui::GetIO().DisplaySize.x;
    int screenH = ImGui::GetIO().DisplaySize.y;
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();

    // 1. PEGAR A MATRIZ DA CÂMERA
    uintptr_t camera = Read<uintptr_t>(base + OFFSET_CAMERA_MAIN);
    if (!camera) return;
    float* viewMatrix = (float*)(camera + OFFSET_CAMERA_MATRIX);

    // 2. PEGAR A LISTA DE JOGADORES
    uintptr_t playerList = Read<uintptr_t>(base + OFFSET_PLAYER_LIST);
    if (!playerList) return;
    
    int playerCount = Read<int>(playerList + OFFSET_LIST_COUNT); 
    uintptr_t playersArray = Read<uintptr_t>(playerList + OFFSET_LIST_ITEMS);
    if (!playersArray || playerCount <= 0 || playerCount > 50) return;

    // 3. ITERAR SOBRE OS JOGADORES
    for (int i = 0; i < playerCount; i++) {
        uintptr_t player = Read<uintptr_t>(playersArray + (i * 0x8));
        if (!player) continue;

        // Ignorar o próprio jogador e os mortos
        if (Read<bool>(player + OFFSET_IS_LOCAL_PLAYER)) continue;
        if (Read<bool>(player + OFFSET_IS_DEAD)) continue;

        // Posições de cabeça e pé para a caixa
        Vector3 headPos = Read<Vector3>(player + OFFSET_HEAD_POS);
        Vector3 feetPos = Read<Vector3>(player + OFFSET_FEET_POS);

        Vector2 headScreen, feetScreen;
        if (WorldToScreen(headPos, headScreen, viewMatrix, screenW, screenH) &&
            WorldToScreen(feetPos, feetScreen, viewMatrix, screenW, screenH)) {
            
            // --- CAIXA (BOX) ---
            float boxHeight = feetScreen.y - headScreen.y;
            float boxWidth = boxHeight / 2.0f; 

            ImU32 boxColor = IM_COL32(0, 255, 0, 255); // Verde

            drawList->AddRect(
                ImVec2(headScreen.x - boxWidth / 2, headScreen.y),
                ImVec2(headScreen.x + boxWidth / 2, feetScreen.y),
                boxColor, 0.0f, 0, 1.5f
            );

            // --- VIDA (HEALTH) ---
            float health = Read<float>(player + OFFSET_HEALTH);
            float maxHealth = 100.0f;

            if (health > 0.0f && health <= maxHealth) {
                // Barra de vida ao lado esquerdo da caixa
                float barWidth = 3.0f;
                float barX = headScreen.x - (boxWidth / 2) - 6.0f;
                
                // Fundo da barra (Preto)
                drawList->AddRectFilled(
                    ImVec2(barX, headScreen.y),
                    ImVec2(barX + barWidth, feetScreen.y),
                    IM_COL32(0, 0, 0, 255)
                );

                // Vida atual
                float healthPercentage = health / maxHealth;
                float healthHeight = boxHeight * healthPercentage;
                
                ImU32 healthColor = IM_COL32(
                    (int)(255 * (1.0f - healthPercentage)),
                    (int)(255 * healthPercentage),
                    0, 255
                );

                drawList->AddRectFilled(
                    ImVec2(barX, feetScreen.y - healthHeight),
                    ImVec2(barX + barWidth, feetScreen.y),
                    healthColor
                );

                // Texto da vida
                char healthText[32];
                sprintf(healthText, "%.0f", health);
                drawList->AddText(
                    ImVec2(barX - 20, feetScreen.y - (healthHeight / 2) - 7),
                    IM_COL32(255, 255, 255, 255),
                    healthText
                );
            }

            // --- ESQUELETO (SKELETON) ---
            uintptr_t boneArray = Read<uintptr_t>(player + OFFSET_BONE_ARRAY);
            if (boneArray) {
                Vector3 head = Read<Vector3>(boneArray + (BONE_HEAD * 0x30) + 0x38);
                Vector3 neck = Read<Vector3>(boneArray + (BONE_NECK * 0x30) + 0x38);
                Vector3 pelvis = Read<Vector3>(boneArray + (BONE_PELVIS * 0x30) + 0x38);
                
                Vector2 scrHead, scrNeck, scrPelvis;
                
                if (WorldToScreen(head, scrHead, viewMatrix, screenW, screenH) &&
                    WorldToScreen(neck, scrNeck, viewMatrix, screenW, screenH) &&
                    WorldToScreen(pelvis, scrPelvis, viewMatrix, screenW, screenH)) {
                    
                    // Cabeça ao pescoço
                    drawList->AddLine(ImVec2(scrHead.x, scrHead.y), ImVec2(scrNeck.x, scrNeck.y), IM_COL32(255, 255, 255, 255), 2.0f);
                    // Pescoço à pélvis
                    drawList->AddLine(ImVec2(scrNeck.x, scrNeck.y), ImVec2(scrPelvis.x, scrPelvis.y), IM_COL32(255, 255, 255, 255), 2.0f);
                }
            }
        }
    }
}
