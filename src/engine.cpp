#define _SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <filesystem>
#include <cstdlib> // For exit
#include <cstdio>
#include <vector>
#include<map>
#include <optional>



#include "glm/glm.hpp"
#include "MapHelper.h"
#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"



#include "SDL2/SDL.h"

#include "SDL_image/SDL_image.h"
#include "SDL_ttf/SDL_ttf.h"
#include "SDL_mixer/SDL_mixer.h"

#include "Helper.h"
#include "AudioHelper.h"

int health = 3;
int score = 0;
int render_width = 640;
int render_height = 360;
int nextActorId = 0;

int lastDamageFrame = 0; // Frame number when the last damage was taken
const int damageCooldown = 180; // Number of frames to wait before taking damage again


int clear_color_r = 255;
int clear_color_b = 255;
int clear_color_g = 255;

std::string game_start_message_mod, game_over_bad_message_mod, game_over_good_message_mod;
std::vector<bool> hasTriggeredScoreIncreaseForActor;
std::string gameplayAudioPath;

std::string nextSceneName = "";
glm::ivec2 player_position;

struct GameConfig{
    std::string heartPath;
    bool hasPlayer = false;
    
    float cam_offset_x = 0.0f;
    float cam_offset_y = 0.0f;
    
    int player_position_x;
    int player_position_y;
    
    bool needsSorting = false;
};
GameConfig gameConfig;

struct IntroScreenState {
    std::vector<SDL_Texture*> images;
    std::vector<SDL_Texture*> introTexts;
    std::vector<std::string> texts;
    size_t currentImageIndex = 0;
    size_t currentTextIndex = 0;
    TTF_Font* font = nullptr;
    std::string musicPath;
    std::string fontPath;
    
};

IntroScreenState introScreenState;

struct Actor {
    int id;
    std::string name = "";
    char view = '?';
    glm::ivec2 transform_position = {0, 0};
    float vel_x = 0, vel_y = 0;
    bool blocking = false;
    std::string nearby_dialogue = "";
    std::string contact_dialogue = "";
    
    //new properties
    std::string view_image; // Path to the actor's image
    glm::vec2 transform_scale = {1.0f, 1.0f}; // Scale factors
    double transform_rotation_degrees = 0.0; // Rotation in degrees
    glm::vec2 view_pivot_offset = {};
    
    std::optional<int> render_order = std::nullopt;
    
    Actor() {}


};

std::vector<Actor> actors_custom;
std::map<std::string, SDL_Texture*> textureMap;
std::map<std::string, Actor> actorTemplates;

bool movedOnFromInit = false;

void loadRenderingConfig() {
    std::string configFilePath = "resources/rendering.config";

    if (std::filesystem::exists(configFilePath)) {
        // Open the file
        FILE* fp = fopen(configFilePath.c_str(), "rb");
        if (!fp) {
            return;
        }

        char readBuffer[65536];
        rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));

        rapidjson::Document d;
        d.ParseStream(is);
        fclose(fp);

        // Load x_resolution if it exists
        if (d.HasMember("x_resolution") && d["x_resolution"].IsInt()) {
            render_width = d["x_resolution"].GetInt();
        }

        // Load y_resolution if it exists
        if (d.HasMember("y_resolution") && d["y_resolution"].IsInt()) {
            render_height = d["y_resolution"].GetInt();
        }
        
        if (d.HasMember("clear_color_r") && d["clear_color_r"].IsInt()) {
            clear_color_r = d["clear_color_r"].GetInt();
        }

        if (d.HasMember("clear_color_b") && d["clear_color_b"].IsInt()) {
            clear_color_b = d["clear_color_b"].GetInt();
        }
        if (d.HasMember("clear_color_g") && d["clear_color_g"].IsInt()) {
            clear_color_g = d["clear_color_g"].GetInt();
        }
        
        if (d.HasMember("cam_offset_x") ) {
            gameConfig.cam_offset_x = d["cam_offset_x"].GetFloat();
        }
        if (d.HasMember("cam_offset_y") ) {
            gameConfig.cam_offset_y = d["cam_offset_y"].GetFloat();
        }

        
    }


}
 
void loadIntroScreenResources(SDL_Renderer* renderer) {
    std::string configFilePath = "resources/game.config";
    
    if (std::filesystem::exists(configFilePath)) {
        FILE* fp = fopen(configFilePath.c_str(), "rb");
        if (!fp) {
            return;
        }
        
        char readBuffer[65536];
        rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
        
        rapidjson::Document config;
        config.ParseStream(is);
        fclose(fp);
        
        if (config.HasMember("hp_image") && config["hp_image"].IsString()) {
            std::string imagePath = "resources/images/" + std::string(config["hp_image"].GetString()) + ".png";
            
               // SDL_Texture* texture = IMG_LoadTexture(renderer, imagePath.c_str());
            gameConfig.heartPath = imagePath;
        }
        
        std::string introBGMPath;
        if (config.HasMember("intro_bgm") && config["intro_bgm"].IsString()) {
            std::string bgmName = config["intro_bgm"].GetString();
            if (std::filesystem::exists("resources/audio/" + bgmName + ".wav")) {
                introBGMPath = "resources/audio/" + bgmName + ".wav";
            } else if (std::filesystem::exists("resources/audio/" + bgmName + ".ogg")) {
                introBGMPath = "resources/audio/" + bgmName + ".ogg";
            } else {
                std::cout << "error: failed to play audio clip " << bgmName;
                exit(0);
            }
            introScreenState.musicPath = introBGMPath;
        }
        
        std::string gameplayAudioPathIm;
        if (config.HasMember("gameplay_audio") && config["gameplay_audio"].IsString()) {
            std::string bgmName = config["gameplay_audio"].GetString();
            if (std::filesystem::exists("resources/audio/" + bgmName + ".wav")) {
                gameplayAudioPathIm = "resources/audio/" + bgmName + ".wav";
            } else if (std::filesystem::exists("resources/audio/" + bgmName + ".ogg")) {
                gameplayAudioPathIm = "resources/audio/" + bgmName + ".ogg";
            } else {
                std::cout << "error: failed to play audio clip " << bgmName;
                exit(0);
            }
            //figure out where to put this !
            gameplayAudioPath = gameplayAudioPathIm;
        }
        
        
        if (config.HasMember("intro_image") && config["intro_image"].IsArray()) {
            const auto& images = config["intro_image"];
            for (rapidjson::SizeType i = 0; i < images.Size(); i++) {
                std::string imagePath = "resources/images/" + std::string(images[i].GetString()) + ".png";
                SDL_Texture* texture = IMG_LoadTexture(renderer, imagePath.c_str());
                if (texture == nullptr) {
                    std::cout << "error: missing image " << images[i].GetString();
                    exit(0);
                }
                introScreenState.images.push_back(texture);
            }
            

                
                
            
            TTF_Font* font = nullptr;
            SDL_Color textColor = {255, 255, 255, 255};
            
            if (config.HasMember("font")) {
                std::string fontName = config["font"].GetString();
                std::string fontPath = "resources/fonts/" + fontName + ".ttf";
                
                if (!std::filesystem::exists(fontPath)) {
                    std::cout << "error: font " << fontName << " missing";
                    exit(0);
                }
                TTF_Init();
                font = TTF_OpenFont(fontPath.c_str(), 16);
                introScreenState.font = TTF_OpenFont(fontPath.c_str(), 16);
                introScreenState.fontPath = fontPath;

            } else if (config.HasMember("intro_text")) {
                std::cout << "error: text render failed. No font configured";
                exit(0);
            }
            
            if (font && config.HasMember("intro_text") && config["intro_text"].IsArray()) {
                const auto& texts = config["intro_text"];
                for (rapidjson::SizeType i = 0; i < texts.Size(); i++) {
                    std::string text = texts[i].GetString();
                    introScreenState.texts.push_back(text);
                    SDL_Surface* surface = TTF_RenderText_Solid(font, text.c_str(), textColor);
        
                    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, surface);
                    SDL_FreeSurface(surface);
                    introScreenState.introTexts.push_back(textTexture);
                }
            }
            

   
            if (font) {
                TTF_CloseFont(font);
            }
        }

    }
}

static std::string obtain_word_after_phrase(const std::string& input, const std::string& phrase) {
    size_t pos = input.find(phrase);

    if (pos == std::string::npos) return "";

    pos += phrase.length();
    while (pos < input.size() && std::isspace(input[pos])) {
        ++pos;
    }

    if (pos == input.size()) return "";
    size_t endPos = pos;
    while (endPos < input.size() && !std::isspace(input[endPos])) {
        ++endPos;
    }

    return input.substr(pos, endPos - pos);
}

std::string fetchInitialScene() {
    FILE* configFile = fopen("resources/game.config", "rb");


    char readBuffer[65536];
    rapidjson::FileReadStream is(configFile, readBuffer, sizeof(readBuffer));

    rapidjson::Document configDoc;
    configDoc.ParseStream(is);
    fclose(configFile);

    if (!configDoc.HasMember("initial_scene")) {
        std::cout << "error: initial_scene unspecified";
        return "";
    }

    return configDoc["initial_scene"].GetString();
}

std::string fetchGameTitle() {
    FILE* configFile = fopen("resources/game.config", "rb");


    char readBuffer[65536];
    rapidjson::FileReadStream is(configFile, readBuffer, sizeof(readBuffer));

    rapidjson::Document configDoc;
    configDoc.ParseStream(is);
    fclose(configFile);

    if (!configDoc.HasMember("game_title")) {
        return "";
    }

    return configDoc["game_title"].GetString();
}





void applyTemplateToActor(Actor& actor, const std::string& templateName) {
    std::string templateFilePath = "resources/actor_templates/" + templateName + ".template";
    if (!std::filesystem::exists(templateFilePath)) {
        std::cout << "error: template " << templateName << " is missing";
        exit(0);
    }

    FILE* file = fopen(templateFilePath.c_str(), "rb");

    char readBuffer[65536];
    rapidjson::FileReadStream is(file, readBuffer, sizeof(readBuffer));
    rapidjson::Document d;
    d.ParseStream(is);
    fclose(file);

    if (d.HasMember("name")) actor.name = d["name"].GetString();
    if (d.HasMember("view")) actor.view = d["view"].GetString()[0];
    if (d.HasMember("transform_position_x")) actor.transform_position.x = d["transform_position_x"].GetInt()*100;
    if (d.HasMember("transform_position_y")) actor.transform_position.y = d["transform_position_y"].GetInt()*100;

    if (d.HasMember("vel_x")) actor.vel_x = d["vel_x"].GetFloat()*100;
    if (d.HasMember("vel_y")) actor.vel_y = d["vel_y"].GetFloat()*100;
    if (d.HasMember("blocking")) actor.blocking = d["blocking"].GetBool();
    if (d.HasMember("nearby_dialogue")) actor.nearby_dialogue = d["nearby_dialogue"].GetString();
    if (d.HasMember("contact_dialogue")) actor.contact_dialogue = d["contact_dialogue"].GetString();
    
    //new
    
    if (d.HasMember("view_image")) actor.view_image = d["view_image"].GetString();
    

    if (d.HasMember("transform_scale_x")) actor.transform_scale.x = (d["transform_scale_x"].GetFloat());
    else actor.transform_scale.x = 1.0f;

    if (d.HasMember("transform_scale_y")) actor.transform_scale.y = (d["transform_scale_y"].GetFloat());
    else actor.transform_scale.y = 1.0f;

    if (d.HasMember("transform_rotation_degrees")) actor.transform_rotation_degrees = d["transform_rotation_degrees"].GetDouble();
    else actor.transform_rotation_degrees = 0.0;

    if (d.HasMember("view_pivot_offset_x")) actor.view_pivot_offset.x = d["view_pivot_offset_x"].GetDouble();

    if (d.HasMember("view_pivot_offset_y")) actor.view_pivot_offset.y = d["view_pivot_offset_y"].GetDouble();
    
    if (d.HasMember("render_order")) actor.render_order = d["render_order"].GetInt();
    

}





int loadScene(const std::string& sceneName, SDL_Renderer* renderer) {
    gameConfig.hasPlayer = false;

    std::string sceneFilePath = "resources/scenes/" + sceneName + ".scene";
    if (!std::filesystem::exists(sceneFilePath)) {
        std::cout << "error: scene " << sceneName << " is missing";
        exit(0);
    }
    FILE* file = fopen(sceneFilePath.c_str(), "rb");
    char readBuffer[65536];
    rapidjson::FileReadStream is(file, readBuffer, sizeof(readBuffer));
    rapidjson::Document d;
    d.ParseStream(is);
    fclose(file);

    actors_custom.clear();
    

    if (d.HasMember("actors") && d["actors"].IsArray()) {
        const rapidjson::Value& actorsArray = d["actors"];
        for (rapidjson::SizeType i = 0; i < actorsArray.Size(); i++) {
            const rapidjson::Value& actorObject = actorsArray[i];
            Actor actor;
            
            if (actorObject.HasMember("template")) {
                std::string templateName = actorObject["template"].GetString();
                applyTemplateToActor(actor, templateName);
            }
            
            if (actorObject.HasMember("name")) actor.name = actorObject["name"].GetString();

            if (actorObject.HasMember("view")) actor.view = actorObject["view"].GetString()[0];
            if (actorObject.HasMember("transform_position_x")) actor.transform_position.x = actorObject["transform_position_x"].GetInt()*100;
            if (actorObject.HasMember("transform_position_y")) actor.transform_position.y = actorObject["transform_position_y"].GetInt()*100;
            if (actorObject.HasMember("vel_x")) actor.vel_x = actorObject["vel_x"].GetFloat()*100;
            if (actorObject.HasMember("vel_y")) actor.vel_y = actorObject["vel_y"].GetFloat()*100;
            if (actorObject.HasMember("blocking")) actor.blocking = actorObject["blocking"].GetBool();
            if (actorObject.HasMember("nearby_dialogue")) actor.nearby_dialogue = actorObject["nearby_dialogue"].GetString();
            if (actorObject.HasMember("contact_dialogue")) actor.contact_dialogue = actorObject["contact_dialogue"].GetString();
            
            //new
            if (actorObject.HasMember("view_image")) actor.view_image = actorObject["view_image"].GetString();

            

            if (actorObject.HasMember("transform_scale_x")) actor.transform_scale.x = (actorObject["transform_scale_x"].GetFloat());
            else actor.transform_scale.x = 1.0f;

            if (actorObject.HasMember("transform_scale_y")) actor.transform_scale.y = (actorObject["transform_scale_y"].GetFloat());
            else actor.transform_scale.y = 1.0f;

            if (actorObject.HasMember("transform_rotation_degrees")) actor.transform_rotation_degrees = actorObject["transform_rotation_degrees"].GetDouble();
            else actor.transform_rotation_degrees = 0.0;

            if (actorObject.HasMember("view_pivot_offset_x")) actor.view_pivot_offset.x = actorObject["view_pivot_offset_x"].GetDouble();

            if (actorObject.HasMember("view_pivot_offset_y")) actor.view_pivot_offset.y = actorObject["view_pivot_offset_y"].GetDouble();

            if (actorObject.HasMember("render_order")) actor.render_order = actorObject["render_order"].GetInt();
            actor.id = nextActorId;
           // nextActorId++;
            actors_custom.emplace_back(actor);

            if (actor.name == "player") {
                player_position = actor.transform_position;
                gameConfig.player_position_x = player_position.x;
                gameConfig.player_position_y = player_position.y;
                gameConfig.hasPlayer = true;
                if (gameConfig.hasPlayer == true && gameConfig.heartPath.empty()) {
                  std::cout << "error: player actor requires an hp_image be defined";
                  exit(0);
                }

            }
        }
    }
    hasTriggeredScoreIncreaseForActor.resize(actors_custom.size(), false);
    for (auto& actor : actors_custom) {
        if (textureMap.find(actor.view_image) == textureMap.end()) {
            std::string path = "./resources/images/" + actor.view_image + ".png";
            SDL_Texture* texture = IMG_LoadTexture(renderer, path.c_str());
            if (texture) {
                textureMap[actor.view_image] = texture;

            }
        }
    }
    return 1;
}

void drawPlayerScore(SDL_Renderer* renderer,TTF_Font* font, int score) {

    std::string configFilePath = "resources/game.config";
    

        if (std::filesystem::exists(configFilePath)) {
            FILE* fp = fopen(configFilePath.c_str(), "rb");
            if (!fp) {
                return;
            }
            
            char readBuffer[65536];
            rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
            
            rapidjson::Document config;
            config.ParseStream(is);
            fclose(fp);
            if (config.HasMember("font")) {
                std::string fontName = config["font"].GetString();
                std::string fontPath = "resources/fonts/" + fontName + ".ttf";
                introScreenState.fontPath = fontPath;
                
                if (!std::filesystem::exists(fontPath)) {
                    std::cout << "error: font " << fontName << " missing";
                    exit(0);
                }
                TTF_Init();
                font = TTF_OpenFont(fontPath.c_str(), 16);
                
            }
        }
    
        
    SDL_Color textColor = {255, 255, 255, 255}; // white
    std::string scoreText = "score : " + std::to_string(score);

    SDL_Surface* surface = TTF_RenderText_Solid(font, scoreText.c_str(), textColor);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    


    int w = surface->w, h = surface->h;
    SDL_Rect dstRect = {5, 5, w, h}; // at (5,5)
    SDL_RenderCopy(renderer, texture, nullptr, &dstRect);

    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}


void drawDialogue(SDL_Renderer* renderer,TTF_Font* font, std::string message, int m, int i) {

    std::string configFilePath = "resources/game.config";
 
 
        if (std::filesystem::exists(configFilePath)) {
            FILE* fp = fopen(configFilePath.c_str(), "rb");
            if (!fp) {
                return;
            }
            
            char readBuffer[65536];
            rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
            
            rapidjson::Document config;
            config.ParseStream(is);
            fclose(fp);
            if (config.HasMember("font")) {
                std::string fontName = config["font"].GetString();
                std::string fontPath = "resources/fonts/" + fontName + ".ttf";
                introScreenState.fontPath = fontPath;
                
                if (!std::filesystem::exists(fontPath)) {
                    std::cout << "error: font " << fontName << " missing";
                    exit(0);
                }
                TTF_Init();
                font = TTF_OpenFont(fontPath.c_str(), 16);
   
                
            }
        }
    
        
    SDL_Color textColor = {255, 255, 255, 255}; // white


    SDL_Surface* surface = TTF_RenderText_Solid(font, message.c_str(), textColor);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    


    int w = surface->w, h = surface->h;
    SDL_Rect textRect;
    textRect.x = 25;

    textRect.y = (render_height - 50 - ((m-1-i)*50));
    textRect.w = w;
    textRect.h = h;
    

    SDL_RenderCopy(renderer, texture, nullptr, &textRect);

    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}


void drawPlayerHealth(SDL_Renderer* renderer, int health) {
    SDL_Texture* hpTexture = IMG_LoadTexture(renderer, gameConfig.heartPath.c_str());

  //  std::cout<<health;
    int w, h;
    SDL_QueryTexture(hpTexture, nullptr, nullptr, &w, &h);
    for (int i = 0; i < health; ++i) {
         
        SDL_Rect dstRect = {5 + i * (w + 5), 25, w, h};
        SDL_RenderCopy(renderer, hpTexture, nullptr, &dstRect);
    }

    SDL_DestroyTexture(hpTexture);
}



void updateNPCPositions() {


    for (auto& actor : actors_custom) {
    
     
            
            int targetX = actor.transform_position.x + actor.vel_x;
            int targetY = actor.transform_position.y + actor.vel_y;
        
            
            bool actorBlocked = false;
            for (const auto& otherActor : actors_custom) {
                if (&actor != &otherActor && otherActor.blocking && otherActor.transform_position.x == targetX && otherActor.transform_position.y == targetY) {
                    actorBlocked = true;

                    break;
                }
            }
            
            if (actorBlocked) {
                actor.vel_x = -actor.vel_x;
                actor.vel_y = -actor.vel_y;
            }
            else {
                actor.transform_position.x += actor.vel_x;
                actor.transform_position.y += actor.vel_y;

            }
        
    }
}

 
void sortActorsForRendering() {
    std::sort(actors_custom.begin(), actors_custom.end(), [](const Actor& a, const Actor& b) {

        if (a.render_order.has_value() || b.render_order.has_value()) {

            if (!a.render_order.has_value()) return false;
            if (!b.render_order.has_value()) return true;

            if (a.render_order.value() != b.render_order.value()) {
                return a.render_order.value() < b.render_order.value();
            }
        }
        

        if (a.transform_position.y != b.transform_position.y) {
            return a.transform_position.y < b.transform_position.y;
        }

        
        return a.id < b.id;
    });
    gameConfig.needsSorting = false;
}






bool nott = 0;

bool renderMapAroundPlayer(glm::ivec2& player_position, SDL_Renderer* renderer ) {
    
    
    //loadActorTextures(renderer);
    glm::vec2 cameraPosition = {(gameConfig.cam_offset_x * 100) ,(gameConfig.cam_offset_y * 100)};

    
    
    for (auto& actor : actors_custom) {

        if (actor.name == "player") {
            actor.transform_position.x = player_position.x;
            actor.transform_position.y = player_position.y;
            //std::cout << "player pos1: " << actor.transform_position.x <<" " << actor.transform_position.y << std::endl;
            gameConfig.player_position_x = player_position.x;
            gameConfig.player_position_y = player_position.y;
            //std::cout << "player pos2: " << gameConfig.player_position_x <<" " << gameConfig.player_position_x << std::endl;
            
            cameraPosition = glm::vec2(player_position.x + (gameConfig.cam_offset_x*100) ,player_position.y + (gameConfig.cam_offset_y*100));
            
            break;
        }
    }
    
    


    std::vector<std::string> adj_messages(actors_custom.size(), "");

    bool continueGame = true;
    std::string nextScene = nextSceneName;

    std::map<std::pair<int, int>, const Actor*> highestIdActorPerCell;


    for (size_t i = 0; i < actors_custom.size(); ++i) {
        auto& actor = actors_custom[i];
        
        std::pair<int, int> cell(actor.transform_position.x, actor.transform_position.y);
        highestIdActorPerCell[cell] = &actor;
        
        if (std::abs(actor.transform_position.x - gameConfig.player_position_x) <= 100 && std::abs(actor.transform_position.y - gameConfig.player_position_y) <= 100) {
            adj_messages[i] = actor.nearby_dialogue ;
           
        }
        if (actor.transform_position.x == gameConfig.player_position_x && actor.transform_position.y == gameConfig.player_position_y) {
            adj_messages[i] = actor.contact_dialogue ;
        }
    }

    
    if(gameConfig.needsSorting == true){
        sortActorsForRendering();
        gameConfig.needsSorting = false;
    }

    
    for (auto& actor : actors_custom) {
            
        
        
        SDL_Texture* texture = textureMap[actor.view_image];

        //std::string path = "./resources/images/" + actor.view_image + ".png";
        //SDL_Texture* texture = IMG_LoadTexture(renderer, path.c_str());
            
            int textureWidth = 0, textureHeight = 0;
            SDL_QueryTexture(texture, nullptr, nullptr, &textureWidth, &textureHeight);

            int scaledWidth = static_cast<int>(textureWidth * std::abs(actor.transform_scale.x));
            int scaledHeight = static_cast<int>(textureHeight * std::abs(actor.transform_scale.y));

            glm::vec2 scaledPivotOff = {
                std::round((actor.view_pivot_offset.x != 0) ? actor.view_pivot_offset.x * std::abs(actor.transform_scale.x) : static_cast<float>(scaledWidth) / 2),
                std::round((actor.view_pivot_offset.y != 0) ? actor.view_pivot_offset.y * std::abs(actor.transform_scale.y) : static_cast<float>(scaledHeight) / 2)
            };

            SDL_Point pivot = {static_cast<int>(scaledPivotOff.x), static_cast<int>(scaledPivotOff.y)};

            glm::vec2 actorScreenPosition = {
                actor.transform_position.x - cameraPosition.x - scaledPivotOff.x + render_width / 2,
                actor.transform_position.y - cameraPosition.y - scaledPivotOff.y + render_height / 2
            };

            SDL_Rect dstRect = {
                static_cast<int>(actorScreenPosition.x), static_cast<int>(actorScreenPosition.y),
                scaledWidth, scaledHeight
            };

            SDL_Rect srcRect = {0, 0, textureWidth, textureHeight};
            SDL_RendererFlip flip = SDL_FLIP_NONE;
            if (actor.transform_scale.x < 0) flip = (SDL_RendererFlip)(flip | SDL_FLIP_HORIZONTAL);
            if (actor.transform_scale.y < 0) flip = (SDL_RendererFlip)(flip | SDL_FLIP_VERTICAL);

            Helper::SDL_RenderCopyEx498(actor.id, actor.name, renderer, texture, &srcRect, &dstRect, actor.transform_rotation_degrees, &pivot, flip);

            
        }
    

    int actualAdj = 0;
    for (size_t i = 0; i < adj_messages.size(); ++i) {
        if (adj_messages[i].size()>1) {
            ++actualAdj;
        }
    }
    int curr = actualAdj-1;
    for (size_t i = 0; i < adj_messages.size(); ++i) {

        std::string sceneName = obtain_word_after_phrase(adj_messages[i], "proceed to ");
        
        if (adj_messages[i].size()>1) {
            
            drawDialogue(renderer, introScreenState.font, adj_messages[i], actualAdj, curr );
            --curr;
            
            continueGame = [&]() {
                if (adj_messages[i].find("health down") != std::string::npos) {
                    
                    int currentFrame = Helper::GetFrameNumber();
                    if (currentFrame - lastDamageFrame > damageCooldown||currentFrame<damageCooldown) {
                        if(currentFrame<damageCooldown){
                            if(!nott){
                                nott = 1;
                                health -= 1;
                                lastDamageFrame = currentFrame;
                            }
                            
                        }// Check if enough frames have passed
                        else{
                            health -= 1;
                            lastDamageFrame = currentFrame; // Update the frame number when damage was taken
                        }
                    }
                }
                else if (adj_messages[i].find("score up") != std::string::npos) {
  
                    if (!hasTriggeredScoreIncreaseForActor[i]) {
                        score += 1;
                        hasTriggeredScoreIncreaseForActor[i] = true;
  
                    }
                }
                else if (adj_messages[i].find("you win") != std::string::npos) {
                    //std::cout << "health : " << health << ", score : " << score << std::endl;
                    //std::cout << game_over_good_message_mod;
                    return false;
                }
                else if (adj_messages[i].find("game over") != std::string::npos) {
                   // std::cout << "health : " << health << ", score : " << score << std::endl;
                   // std::cout << game_over_bad_message_mod;
                    return false;
                }
                
                else if (adj_messages[i].find("proceed to") != std::string::npos) {
                    std::string sceneName = obtain_word_after_phrase(adj_messages[i], "proceed to ");
                    nextSceneName = sceneName;
                    if (!sceneName.empty()) {
                        
            
                        if (!loadScene(sceneName, renderer)) {
                            std::cout << "error: scene " << sceneName << " is missing" << std::endl;
                            exit(0);
                        } else {
                        
                            movedOnFromInit=true;
                            actors_custom.clear();
                            
                            adj_messages.clear();
                            adj_messages.resize(actors_custom.size());
                
                            loadScene(nextSceneName, renderer);
                            gameConfig.needsSorting = true;
                            
                            return false;
                            
                        }
 
                        
                      
                     
                        
                    }
                }
                return true;
                }();
        }
    }
    if (health <= 0) {
       // std::cout << "health : " << health << ", score : " << score << std::endl;
       // std::cout << game_over_bad_message_mod;
      //  return false;
    }
    if(gameConfig.hasPlayer){
        drawPlayerHealth(renderer, health);
        drawPlayerScore(renderer, introScreenState.font, score);
    }
    

    return continueGame;
}


int main(int argc, char* argv[]) {


    if (!std::filesystem::exists("resources")) {
        std::cout << "error: resources/ missing";
        exit(0);
    }
    
    if (!std::filesystem::exists("resources/game.config")) {
        std::cout << "error: resources/game.config missing";
        exit(0);
    }
    
    FILE* fp = fopen("resources/game.config", "rb");
    char readBuffer[65536];
    rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
    
    rapidjson::Document d;
    d.ParseStream(is);
    fclose(fp);
    
    ///!!!!!!!!!!!!!!!!!
    std::string gameTitle = fetchGameTitle();
    loadRenderingConfig();
    SDL_Window* window = Helper::SDL_CreateWindow498(gameTitle.c_str(),SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,render_width,render_height,SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = Helper::SDL_CreateRenderer498(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_Event e;
    
    AudioHelper::Mix_OpenAudio498(44100, MIX_DEFAULT_FORMAT, 2, 2048);
    
    std::vector<SDL_Texture*> introImages;
    std::vector<std::string> introTexts;
    Mix_Chunk* bgm = nullptr;
    
    
    loadIntroScreenResources(renderer);
    
    if (!introScreenState.musicPath.empty()) {
        bgm = AudioHelper::Mix_LoadWAV498(introScreenState.musicPath.c_str());
        if (bgm != nullptr) {
            
            AudioHelper::Mix_PlayChannel498(0, bgm, -1); // -1 for infinite loop
        }
        
    }
    bool running = true;
    bool audioHalted = false;
    bool bgmStarted = false;
    bool introDone = false;
    
    bool loadedInit = false;
    
    
    SDL_SetRenderDrawColor(renderer, clear_color_r, clear_color_g, clear_color_b,255);
    SDL_RenderClear(renderer);
    
    size_t introSize = 0;
    if(introScreenState.images.size()>introScreenState.texts.size()){
        introSize = introScreenState.images.size();
    }
    else{
        introSize = introScreenState.texts.size();
    }
    

    while (running) {
        

        bool advanceIntro = false;
        int target_x = gameConfig.player_position_x;
        int target_y = gameConfig.player_position_y;
        
        while (Helper::SDL_PollEvent498(&e)) {
            if (e.type == SDL_QUIT) {
                running = false;
                Helper::SDL_RenderPresent498(renderer);
                exit(0);
            }
            // check if advance image sequence
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.scancode == SDL_SCANCODE_SPACE || e.key.keysym.scancode == SDL_SCANCODE_RETURN) {
                    advanceIntro = true;
                }
            }
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                advanceIntro = true;
            }
            // end check if advance image sequence
            // Player movement
            if (e.type == SDL_KEYDOWN) {
                
                switch (e.key.keysym.scancode) {
                    case SDL_SCANCODE_UP:
                        target_y -= 100; // Move up
                        
                        break;
                    case SDL_SCANCODE_DOWN:
                        target_y += 100; // Move down
                        
                        break;
                    case SDL_SCANCODE_LEFT:
                        target_x -= 100; // Move left
                        
                        break;
                    case SDL_SCANCODE_RIGHT:
                        target_x += 100; // Move right
                        
                        break;
                    default:
                        break;
                }
                // std::cout << "Movement applied: " << target_x << ", " << target_y << std::endl;
                
            }
        }
        
        if (advanceIntro) {
            // advance the image index if there's a next image or if texts are still showing
            if (introScreenState.currentImageIndex  < introScreenState.images.size() || introScreenState.currentTextIndex < introScreenState.introTexts.size()) {
                introScreenState.currentImageIndex++;
            }
            // advance the text index if there's a next text or if images are still showing
            if (introScreenState.currentTextIndex  < introScreenState.introTexts.size() || introScreenState.currentImageIndex < introScreenState.images.size()) {
                introScreenState.currentTextIndex++;
            }
        }
        
        SDL_SetRenderDrawColor(renderer, clear_color_r, clear_color_g, clear_color_b, 255);
        SDL_RenderClear(renderer);
        
        // render the last available image if the current image index exceeds the number of images
        if (introScreenState.currentImageIndex  < introScreenState.images.size() ) {
            size_t imgIndex = std::min(introScreenState.currentImageIndex, introScreenState.images.size()-1 );
            SDL_RenderCopy(renderer, introScreenState.images[imgIndex], nullptr, nullptr);
            
        }
        
        // render the last available text if the current text index exceeds the number of texts
        if (introScreenState.currentImageIndex  < introScreenState.images.size() || introScreenState.currentTextIndex  < introScreenState.texts.size()) {
            
            size_t imgIndex = std::min(introScreenState.currentImageIndex, introScreenState.images.size()-1 );
            SDL_RenderCopy(renderer, introScreenState.images[imgIndex], nullptr, nullptr);
            
            if(introScreenState.texts.size()>0){
                size_t textIndex = std::min(introScreenState.currentTextIndex, introScreenState.introTexts.size() -1);
                int textWidth = 0, textHeight = 0;
                TTF_Font* font = introScreenState.font;
                std::string text = introScreenState.texts[textIndex];
                
                TTF_SizeText(font, text.c_str(), &textWidth, &textHeight);
                
                SDL_Rect textRect;
                textRect.x = 25;
                textRect.y = render_height - 50;
                textRect.w = textWidth;
                textRect.h = textHeight;
                
                SDL_RenderCopy(renderer, introScreenState.introTexts[textIndex], nullptr, &textRect);
            }
            
        }

        if (introScreenState.currentImageIndex >= introScreenState.images.size() && introScreenState.currentTextIndex >= introScreenState.introTexts.size()) {
            introDone = true;
            if (!introScreenState.musicPath.empty() && audioHalted == false) {
                AudioHelper::Mix_HaltChannel498(0);
                audioHalted = true;
            }
            
        }
        else{
            Helper::SDL_RenderPresent498(renderer);
        }
        
        

            //////////////!!
            ///
            if (d.HasMember("game_start_message")) {
                game_start_message_mod = d["game_start_message"].GetString();
            }
            
            if (d.HasMember("game_over_bad_message")) {
                game_over_bad_message_mod = d["game_over_bad_message"].GetString();
            }
            
            if (d.HasMember("game_over_good_message")) {
                game_over_good_message_mod = d["game_over_good_message"].GetString();
            }
            
            loadRenderingConfig();
            std::string initialSceneName = fetchInitialScene();
        
            
            if(bgmStarted == false){
                if (!gameplayAudioPath.empty()) {
                    bgm = AudioHelper::Mix_LoadWAV498(gameplayAudioPath.c_str());
                    if (bgm != nullptr) {
                        bgmStarted = true;
                        AudioHelper::Mix_PlayChannel498(0, bgm, -1); // -1 for infinite loop
                    }
                }
            }
            
            if (initialSceneName.empty()) {
                return 0;
            }
            if(movedOnFromInit==false && loadedInit == false){
    
       
                loadedInit = true;

                if (!loadScene(initialSceneName, renderer)) {

                    
                    return 0;
                }
            }
            
            if (d.HasMember("game_start_message")) {
                // std::cout << game_start_message_mod << std::endl;
            }
            
            //lets update player position here?
            
            
            
            // Check for blocking actors
            bool blocked = false;
            for (const auto& actor : actors_custom) {
                if (actor.blocking && actor.transform_position.x == target_x && actor.transform_position.y == target_y) {
                    blocked = true;
                    break;
                }
                
            }
            
            if (!blocked) {
                
                gameConfig.player_position_x = target_x;
                gameConfig.player_position_y = target_y;
                gameConfig.needsSorting = true;
            }
            
            
            //  std::cout << gameConfig.player_position_x << std::endl;
            
            player_position = {gameConfig.player_position_x, gameConfig.player_position_y};
            int currentFrame= Helper::GetFrameNumber();
                if(currentFrame%60 == 0){
                    updateNPCPositions();
                    
                }
        
    
            if (!renderMapAroundPlayer(player_position, renderer)) {
                movedOnFromInit = true;
                
            }
            else{

                if(introDone){


                    Helper::SDL_RenderPresent498(renderer);}

            }
            
            

        
            if (health <= 0) {

                int currentFrame = Helper::GetFrameNumber();
                if (currentFrame - lastDamageFrame > damageCooldown) {
                    std::cout << game_over_bad_message_mod;
                }
                //  break;
            }
            // std::cout << "health : " << health << ", score : " << score << std::endl;
            

            
        
    }
    
            
            
//
//            if (user_input == "quit") {
//                std::cout << game_over_bad_message_mod;
//                running = false;
//            }

            
        
    
    
    return 0;
    
}



