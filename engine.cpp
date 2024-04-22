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
#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"



#include "SDL2/SDL.h"

#include "SDL_image/SDL_image.h"
#include "SDL_ttf/SDL_ttf.h"
#include "SDL_mixer/SDL_mixer.h"

#include "Helper.h"
#include "AudioHelper.h"

#include <opencv2/opencv.hpp>
#include <opencv2/objdetect.hpp>

#include "faceDetection.hpp"




int health = 3;
int score = 0;
int render_width = 640;
int render_height = 360;
int nextActorId = 0;

int lastDamageFrame = 0;
const int damageCooldown = 180;


int clear_color_r = 255;
int clear_color_b = 255;
int clear_color_g = 255;

std::string game_start_message_mod, game_over_bad_message_mod, game_over_good_message_mod;
std::vector<bool> hasTriggeredScoreIncreaseForActor;
std::string gameplayAudioPath;

std::string nextSceneName = "";
glm::vec2 player_position;

struct GameConfig{
    std::string heartPath;
    bool hasPlayer = false;
    
    float cam_offset_x = 0.0f;
    float cam_offset_y = 0.0f;
    
    int player_position_x;
    int player_position_y;
    
    bool needsSorting = false;
    
    bool GameOver = false;
    std::string gameOverBadAudioPath;
    std::string gameOverGoodAudioPath;
    std::string gameOverBadImgPath;
    std::string gameOverGoodImgPath;
    
    float cam_ease_factor = 1.0f;
    float player_movement_speed = 0.02f;
    
    float zoom_factor = 1.0f;
    glm::vec2 cameraPosition;
    
    bool cameraOn = false;
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
    glm::vec2 transform_position = {0, 0};
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

struct Button {
    SDL_Rect rect;  // Button position and size
    SDL_Texture* texture;  // Button image
    std::function<void()> action;  // Action to perform on click
};

std::vector<Button> buttons;

struct CustomAvatar{
    
    bool camera = true;
    std::string base = "";
    std::string outfit = "";
    std::string hair = "";
    std::string bangs = "";
    std::string shirt = "";
    std::string pants = "";
    std::string accessory = "";
    std::string shoes = "";
    
};

CustomAvatar customAvatar;

struct Avatar {
    int baseIndex = 0;
    int hairIndex = 0;
    int bangsIndex = 0;
    int eyesIndex = 0;
    int mouthIndex = 0;
    int outfitIndex = 0;
};
std::vector<std::vector<SDL_Texture*>> imageVectors; // This will hold all category vectors
Avatar avatarIndices;


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
        if(d.HasMember("cam_ease_factor")){
            gameConfig.cam_ease_factor = d["cam_ease_factor"].GetFloat();
        }
        if(d.HasMember("zoom_factor")){
            gameConfig.zoom_factor = d["zoom_factor"].GetFloat();
        }
        
        
    }


}
 
void loadIntroScreenResources(SDL_Renderer* renderer) {
    if (!std::filesystem::exists("resources")) {
        std::cout << "error: resources/ missing";
        exit(0);
    }
    
    if (!std::filesystem::exists("resources/game.config")) {
        std::cout << "error: resources/game.config missing";
        exit(0);
    }
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
            gameConfig.heartPath = imagePath;
        }

        if(config.HasMember("player_movement_speed")){
            gameConfig.player_movement_speed = config["player_movement_speed"].GetFloat();
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
        
        
///////////////////////////////
        if (config.HasMember("game_over_bad_image")){
            std::string imagePath = "resources/images/" + std::string(config["game_over_bad_image"].GetString()) + ".png";

            gameConfig.gameOverBadImgPath = imagePath;
        }
        

        if (config.HasMember("game_over_bad_audio") && config["game_over_bad_audio"].IsString()) {
            std::string bgmName = config["game_over_bad_audio"].GetString();
            if (std::filesystem::exists("resources/audio/" + bgmName + ".wav")) {
                gameConfig.gameOverBadAudioPath = "resources/audio/" + bgmName + ".wav";
            } else if (std::filesystem::exists("resources/audio/" + bgmName + ".ogg")) {
                gameConfig.gameOverBadAudioPath = "resources/audio/" + bgmName + ".ogg";
            } else {
                std::cout << "error: failed to play audio clip " << bgmName;
                exit(0);
            }

        }
        
        if (config.HasMember("game_over_good_image") && config["game_over_good_image"].IsString()) {
            std::string imagePath = "resources/images/" + std::string(config["game_over_good_image"].GetString()) + ".png";
            
            gameConfig.gameOverGoodImgPath = imagePath;
        }
        

        if (config.HasMember("game_over_good_audio") && config["game_over_good_audio"].IsString()) {
            std::string bgmName = config["game_over_good_audio"].GetString();
            if (std::filesystem::exists("resources/audio/" + bgmName + ".wav")) {
                gameConfig.gameOverGoodAudioPath = "resources/audio/" + bgmName + ".wav";
            } else if (std::filesystem::exists("resources/audio/" + bgmName + ".ogg")) {
                gameConfig.gameOverGoodAudioPath = "resources/audio/" + bgmName + ".ogg";
            } else {
                std::cout << "error: failed to play audio clip " << bgmName;
                exit(0);
            }
        }
        /////////////////
        ///        //custom avatar features
        
        if (config.HasMember("base") && config["base"].IsString()) {
            customAvatar.base = "resources/images/" + std::string(config["base"].GetString()) + ".png";
        }

        if (config.HasMember("hair") && config["hair"].IsString()) {
            customAvatar.hair = "resources/images/" + std::string(config["hair"].GetString()) + ".png";
        }

        if (config.HasMember("bangs") && config["bangs"].IsString()) {
            customAvatar.bangs = "resources/images/" + std::string(config["bangs"].GetString()) + ".png";
        }

        if (config.HasMember("shirt") && config["shirt"].IsString()) {
            customAvatar.shirt = "resources/images/" + std::string(config["shirt"].GetString()) + ".png";
        }

        if (config.HasMember("pants") && config["pants"].IsString()) {
            customAvatar.pants = "resources/images/" + std::string(config["pants"].GetString()) + ".png";
        }

        if (config.HasMember("accessory") && config["accessory"].IsString()) {
            customAvatar.accessory = "resources/images/" + std::string(config["accessory"].GetString()) + ".png";
        }

        if (config.HasMember("shoes") && config["shoes"].IsString()) {
            customAvatar.shoes = "resources/images/" + std::string(config["shoes"].GetString()) + ".png";
        }
        if (config.HasMember("outfit") && config["outfit"].IsString()) {
            customAvatar.outfit = "resources/images/" + std::string(config["outfit"].GetString()) + ".png";
        }
        
        
        
        
        
        
        /////////////////

        
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

    SDL_RenderSetScale(renderer, 1.0f, 1.0f);
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
    SDL_RenderSetScale(renderer, 1.0f, 1.0f);
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
        
//        if(a.name != "tile" && b.name != "tile"){
//            std::cout << a.name << " " <<  a.transform_position.y << " "<<  b.name << b.transform_position.y <<std::endl;
//        }
        
//        if(a.transform_position.y == b.transform_position.y){
//                return a.render_order < b.render_order;
//
//        }
        if (a.render_order.has_value() || b.render_order.has_value()) {

            if (!a.render_order.has_value()) return false;
            if (!b.render_order.has_value()) return true;

            if (a.render_order.value() != b.render_order.value() || a.transform_position.y == b.transform_position.y) {
                return a.render_order.value() < b.render_order.value();
            }
        }
        
        if(a.transform_position == b.transform_position){
            return a.transform_position.y > b.transform_position.y;
        }
        else{
            return a.transform_position.y < b.transform_position.y;
        }
        

        return a.transform_position.y < b.transform_position.y;
 
    });
    gameConfig.needsSorting = false;
}






bool nott = 0;
bool end_called = false;
bool done = false;

SDL_Texture* gameOverTexture;

bool renderMapAroundPlayer(glm::vec2& player_position, SDL_Renderer* renderer, SDL_Texture* playerFaceTexture ) {
    
    
    //loadActorTextures(renderer);


    
    
    for (auto& actor : actors_custom) {

        if (actor.name == "player") {
            actor.transform_position.x = player_position.x;
            actor.transform_position.y = player_position.y;
            gameConfig.player_position_x = player_position.x;
            gameConfig.player_position_y = player_position.y;


            


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
        if (actor.transform_position.x == gameConfig.player_position_x  && actor.transform_position.y == gameConfig.player_position_y) {
            adj_messages[i] = actor.contact_dialogue ;
        }
    }

    
    for (auto& actor : actors_custom) {
        
        SDL_Texture* texture = textureMap[actor.view_image];
        SDL_RenderSetScale(renderer, gameConfig.zoom_factor, gameConfig.zoom_factor);
        
        int textureWidth = 0, textureHeight = 0;
        SDL_QueryTexture(texture, nullptr, nullptr, &textureWidth, &textureHeight);
        
        int scaledWidth = static_cast<int>(textureWidth);
        int scaledHeight = static_cast<int>(textureHeight);
        
        glm::vec2 scaledPivotOff = {
            std::round((actor.view_pivot_offset.x != 0) ? actor.view_pivot_offset.x  : static_cast<float>(scaledWidth) / 2),
            std::round((actor.view_pivot_offset.y != 0) ? actor.view_pivot_offset.y  : static_cast<float>(scaledHeight) / 2)
        };
        
        SDL_Point pivot = {static_cast<int>(scaledPivotOff.x), static_cast<int>(scaledPivotOff.y)};
        
        glm::vec2 actorScreenPosition = {
            (actor.transform_position.x - gameConfig.cameraPosition.x - scaledPivotOff.x + (render_width/2)),
            (actor.transform_position.y - gameConfig.cameraPosition.y - scaledPivotOff.y + (render_height/2))
        };
        
        SDL_Rect dstRect = {
            static_cast<int>(actorScreenPosition.x), static_cast<int>(actorScreenPosition.y),
            scaledWidth, scaledHeight
        };
        
        SDL_Rect srcRect = {0, 0, textureWidth, textureHeight};
        SDL_RendererFlip flip = SDL_FLIP_NONE;
        if (actor.transform_scale.x < 0) flip = (SDL_RendererFlip)(flip | SDL_FLIP_HORIZONTAL);
        if (actor.transform_scale.y < 0) flip = (SDL_RendererFlip)(flip | SDL_FLIP_VERTICAL);
        
        if(actor.name != "player"){
            Helper::SDL_RenderCopyEx498(actor.id, actor.name, renderer, texture, &srcRect, &dstRect, actor.transform_rotation_degrees, &pivot, flip);}
        
        if(actor.name == "player" ){
                        int headWidth = 80;
                        int headHeight = 80;
                        SDL_Rect rect;
                        rect.w = headWidth;
                        rect.h = headHeight;
                        rect.x = static_cast<int>(actorScreenPosition.x);
                        rect.y = static_cast<int>(actorScreenPosition.y);

            
            if (!imageVectors[2].empty()) SDL_RenderCopy(renderer, imageVectors[2][avatarIndices.hairIndex], NULL, &rect);
            if (!imageVectors[0].empty()) SDL_RenderCopy(renderer, imageVectors[0][avatarIndices.baseIndex], NULL, &rect);
            if(!gameConfig.cameraOn){
                if (!imageVectors[3].empty()) SDL_RenderCopy(renderer, imageVectors[3][avatarIndices.eyesIndex], NULL, &rect);
                if (!imageVectors[4].empty()) SDL_RenderCopy(renderer, imageVectors[4][avatarIndices.mouthIndex], NULL, &rect);
            }
            
            SDL_Rect rect1 = { static_cast<int>(actorScreenPosition.x) + 14, static_cast<int>(actorScreenPosition.y), 53 , 53};
            
            if(gameConfig.cameraOn){
                SDL_RenderCopy(renderer, playerFaceTexture, NULL, &rect1);
            }
            if (!imageVectors[1].empty()) SDL_RenderCopy(renderer, imageVectors[1][avatarIndices.bangsIndex], NULL, &rect);
            if (!imageVectors[5].empty()) SDL_RenderCopy(renderer, imageVectors[5][avatarIndices.outfitIndex], NULL, &rect);
    }
            SDL_RenderSetScale(renderer, 1.0f, 1.0f);

            
        }
    
    
    

    int actualAdj = 0;
    for (size_t i = 0; i < adj_messages.size(); ++i) {
        if (adj_messages[i].size()>1) {
            ++actualAdj;
        }
    }
    int curr = 0;
    for (size_t i = 0; i < adj_messages.size(); ++i) {

        std::string sceneName = obtain_word_after_phrase(adj_messages[i], "proceed to ");
        
        if (adj_messages[i].size()>1) {
            
            drawDialogue(renderer, introScreenState.font, adj_messages[i], actualAdj, curr );
            ++curr;
            
            continueGame = [&]() {
              if (adj_messages[i].find("health down") != std::string::npos) {
                    
                    int currentFrame = Helper::GetFrameNumber();
                    if (currentFrame - lastDamageFrame >= damageCooldown||currentFrame<damageCooldown) {
                        if(currentFrame<damageCooldown){
                            if(!nott){
                                nott = 1;
                                health -= 1;
                                lastDamageFrame = currentFrame;
                            }
                            
                        }
                        else{
                            health -= 1;
                            lastDamageFrame = currentFrame;
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
                    gameConfig.GameOver = true;
                }
                else if (adj_messages[i].find("game over") != std::string::npos) {

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
                            gameConfig.player_position_x = player_position.x;
                            gameConfig.player_position_y = player_position.y;
                            gameConfig.needsSorting = true;
                            
                            return false;
                            
                        }
 
                        
                      
                     
                        
                    }
                }
                return true;
                }();
        }
    }
    if(gameConfig.GameOver){
        if(!done){
            AudioHelper::Mix_HaltChannel498(0);
            done = true;
        }
        gameConfig.GameOver = true;
        actors_custom.clear();
        adj_messages.clear();
        
        SDL_Texture* gameOverTexture = IMG_LoadTexture(renderer, gameConfig.gameOverGoodImgPath.c_str());
        SDL_RenderCopy(renderer, gameOverTexture ,nullptr,nullptr);
        Mix_Chunk* bgm = nullptr;
        
        bgm = AudioHelper::Mix_LoadWAV498(gameConfig.gameOverGoodAudioPath.c_str());
        if (bgm != nullptr && end_called == false) {
            end_called = true;
            AudioHelper::Mix_PlayChannel498(0, bgm, 0);
        }
    }
    if (health <= 0) {
        if(!done){
            AudioHelper::Mix_HaltChannel498(0);
            done = true;
            gameConfig.GameOver = true;
            actors_custom.clear();
            adj_messages.clear();
            gameOverTexture = IMG_LoadTexture(renderer, gameConfig.gameOverBadImgPath.c_str());
        }
        
        
        SDL_RenderCopy(renderer, gameOverTexture ,nullptr,nullptr);
        Mix_Chunk* bgm = nullptr;
        
        bgm = AudioHelper::Mix_LoadWAV498(gameConfig.gameOverBadAudioPath.c_str());
        if (bgm != nullptr && end_called == false) {
            end_called = true;
            AudioHelper::Mix_PlayChannel498(0, bgm, 0);
        }
    }
    if(gameConfig.hasPlayer && gameConfig.GameOver == false){
        drawPlayerHealth(renderer, health);
        drawPlayerScore(renderer, introScreenState.font, score);
    }
    

    return continueGame;
}

void createButton(const char* imagePath, SDL_Renderer* renderer, int x, int y, int w, int h, std::function<void()> onClick) {
    Button button;
    button.rect = {x, y, w, h};
    button.texture = IMG_LoadTexture(renderer, imagePath);
    std::cout<< "loaded button texture: " << imagePath << std::endl;
    button.action = onClick;
    buttons.push_back(button);
}



SDL_Texture* loadTexture(SDL_Renderer* renderer, const std::string& path) {
    SDL_Surface* surface = IMG_Load(path.c_str());
    if (!surface) {
       // std::cerr << "Failed to load image: " << path << ", error: " << IMG_GetError() << std::endl;
        return nullptr;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if (!texture) {
        std::cerr << "Failed to create texture: " << path << ", error: " << SDL_GetError() << std::endl;
    }
    return texture;
}

namespace fs = std::filesystem;


void loadImagesFromDirectory(const std::string& directory, std::vector<SDL_Texture*>& imageVector, SDL_Renderer* renderer) {
    for (const auto& entry : fs::directory_iterator(directory)) {
        if (entry.is_regular_file()) {
            std::string filePath = entry.path().string();
            SDL_Texture* texture = loadTexture(renderer, filePath);
            if (texture) {
                imageVector.push_back(texture);
            }
        }
    }
}


void loadAvatarImageOptions(SDL_Renderer* renderer,
                            std::vector<SDL_Texture*>& baseImages,
                            std::vector<SDL_Texture*>& hairImages,
                            std::vector<SDL_Texture*>& bangsImages,
                            std::vector<SDL_Texture*>& eyesImages,
                            std::vector<SDL_Texture*>& mouthImages,
                            std::vector<SDL_Texture*>& outfitImages) {
    
    loadImagesFromDirectory("resources/images/base", baseImages, renderer);
    
    loadImagesFromDirectory("resources/images/bangs", bangsImages, renderer);
    loadImagesFromDirectory("resources/images/hair", hairImages, renderer);
    loadImagesFromDirectory("resources/images/eyes", eyesImages, renderer);
    loadImagesFromDirectory("resources/images/mouth", mouthImages, renderer);
    loadImagesFromDirectory("resources/images/outfit", outfitImages, renderer);
}

std::vector<Button> menuButtons;
SDL_Texture* backgroundTexture;


void loadMainMenuResources(SDL_Renderer* renderer) {
    backgroundTexture = IMG_LoadTexture(renderer, "resources/images/menu_background.png");
    SDL_Texture* startButtonTexture = IMG_LoadTexture(renderer, "resources/images/start_button.png");
    SDL_Texture* avatarButtonTexture = IMG_LoadTexture(renderer, "resources/images/avatar_button.png");
    
    int textureWidth = 0, textureHeight = 0;
    SDL_QueryTexture(startButtonTexture, nullptr, nullptr, &textureWidth, &textureHeight);

    menuButtons.push_back({{render_width/2 - (textureWidth/2), render_height/2 - textureHeight/2 - (textureHeight/2), textureWidth, textureHeight}, startButtonTexture, []() {
        std::cout << "Start Game clicked" << std::endl;

    }});
    SDL_QueryTexture(avatarButtonTexture, nullptr, nullptr, &textureWidth, &textureHeight);
    menuButtons.push_back({{render_width/2 - (textureWidth/2), render_height/2 + textureHeight/2 - (textureHeight/2), textureWidth, textureHeight}, avatarButtonTexture, []() {
        std::cout << "Build Your Avatar clicked" << std::endl;

    }});
}

void renderMainMenu(SDL_Renderer* renderer) {
    SDL_RenderCopy(renderer, backgroundTexture, NULL, NULL);
    for (const auto& button : menuButtons) {
        SDL_RenderCopy(renderer, button.texture, NULL, &button.rect);
    }
    Helper::SDL_RenderPresent498(renderer);
}

std::vector<SDL_Texture*> categoryOverlays;

void loadCategoryOverlays(SDL_Renderer* renderer) {
    for (int i = 1; i <= 7; ++i) {
        std::string path = "resources/images/avatar_category_" + std::to_string(i) + ".png";
        SDL_Texture* overlayTexture = IMG_LoadTexture(renderer, path.c_str());
        if (overlayTexture == nullptr) {
            std::cerr << "Failed to load overlay image: " << path << ", error: " << IMG_GetError() << std::endl;
        } else {
            categoryOverlays.push_back(overlayTexture);
            
        }
    }
}


void renderAvatarMaker(SDL_Renderer* renderer, int selectedCategory, SDL_Texture* playerFaceTexture) {
    SDL_RenderClear(renderer);
    
    SDL_Texture* avBack = IMG_LoadTexture(renderer, "resources/images/avatar_background.png");
    SDL_RenderCopy(renderer, avBack, NULL, NULL);
    
    int textureWidth = 0, textureHeight = 0;
    SDL_QueryTexture(imageVectors[0][avatarIndices.baseIndex], nullptr, nullptr, &textureWidth, &textureHeight);

    SDL_Rect rect = {render_width/2 - (textureWidth/2), render_height/2  - (textureHeight/4), textureWidth, textureHeight};
    SDL_Rect rect1 = {render_width/2 - (textureWidth/4) - 7 , render_height/2  - (textureHeight/4), int(textureWidth/1.5), int(textureHeight/1.5)};



    if (!imageVectors[2].empty()) SDL_RenderCopy(renderer, imageVectors[2][avatarIndices.hairIndex], NULL, &rect);

    if (!imageVectors[0].empty()) SDL_RenderCopy(renderer, imageVectors[0][avatarIndices.baseIndex], NULL, &rect);
    if(!gameConfig.cameraOn){
        if (!imageVectors[3].empty()) SDL_RenderCopy(renderer, imageVectors[3][avatarIndices.eyesIndex], NULL, &rect);
        if (!imageVectors[4].empty()) SDL_RenderCopy(renderer, imageVectors[4][avatarIndices.mouthIndex], NULL, &rect);
    }
    
    
    
    if(gameConfig.cameraOn){
        SDL_RenderCopy(renderer, playerFaceTexture, NULL, &rect1);
    }
    if (!imageVectors[1].empty()) SDL_RenderCopy(renderer, imageVectors[1][avatarIndices.bangsIndex], NULL, &rect);
    if (!imageVectors[5].empty()) SDL_RenderCopy(renderer, imageVectors[5][avatarIndices.outfitIndex], NULL, &rect);
    
    if (selectedCategory != -1 && selectedCategory < categoryOverlays.size()) {
        SDL_Texture* overlay = categoryOverlays[selectedCategory];
        if (overlay != nullptr) {
            SDL_RenderCopy(renderer, overlay, NULL, NULL);
        }
    }
    


    Helper::SDL_RenderPresent498(renderer);
}


void changePart(int category, int direction) {
    int size = int(imageVectors[category].size());
    int& currentIndex = *(&avatarIndices.baseIndex + category);
    currentIndex = (currentIndex + direction + size) % size;
}

int selectedCategory = 0;



int main(int argc, char* argv[]) {

    
    cv::VideoCapture cap(2);
    if (!cap.isOpened()) {
        std::cerr << "Error opening video stream or file" << std::endl;
        return -1;
    }
    cv::CascadeClassifier face_cascade = loadFaceCascade("haarcascade_frontalface_default.xml");


    std::string gameTitle = fetchGameTitle();
    loadRenderingConfig();
    SDL_Window* window = Helper::SDL_CreateWindow498(gameTitle.c_str(),SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,render_width,render_height,SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = Helper::SDL_CreateRenderer498(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_Event e;
    
   
    AudioHelper::Mix_OpenAudio498(44100, MIX_DEFAULT_FORMAT, 2, 2048);
    
    std::vector<SDL_Texture*> introImages;
    std::vector<std::string> introTexts;
    Mix_Chunk* bgm = nullptr;
    
    std::vector<SDL_Texture*> baseImages;
    std::vector<SDL_Texture*> hairImages;
    std::vector<SDL_Texture*> bangsImages;
    std::vector<SDL_Texture*> eyesImages;
    std::vector<SDL_Texture*> mouthImages;
    std::vector<SDL_Texture*> outfitImages;
    
    
    loadAvatarImageOptions(renderer, baseImages, bangsImages, hairImages,  eyesImages, mouthImages, outfitImages);
    imageVectors = {  baseImages, hairImages, bangsImages, eyesImages, mouthImages, outfitImages};
    loadIntroScreenResources(renderer);
    loadCategoryOverlays(renderer);

    
    
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
    

    loadMainMenuResources(renderer);  // Load all textures

    bool showMenu = true;
    bool showAvatarMaker = false;

    while (running) {


        bool advanceIntro = false;
        
        glm::vec2 player_position(gameConfig.player_position_x, gameConfig.player_position_y);
        float player_movement_speed = gameConfig.player_movement_speed * 100;
        glm::vec2 movement_direction(0.0f, 0.0f);
        const Uint8 *state = SDL_GetKeyboardState(NULL);
        
        SDL_RenderClear(renderer);
        
        SDL_Texture* playerFaceTexture = nullptr;
        
        if(gameConfig.cameraOn){
        cv::Mat frame;
            cap >> frame;
            if (!frame.empty()) {
                cv::Mat frame_gray;
                cv::cvtColor(frame, frame_gray, cv::COLOR_BGR2GRAY);
                cv::equalizeHist(frame_gray, frame_gray);
                std::vector<cv::Rect> faces;
                face_cascade.detectMultiScale(frame_gray, faces, 1.1, 2, 0 | cv::CASCADE_SCALE_IMAGE, cv::Size(50, 50));
                
                
                std::string redBangs = "./resources/images/redBangs.png";
                if (!faces.empty()) {
                    cv::Mat face_roi = frame(faces[0]);
                    playerFaceTexture = processFrame(frame, faces, renderer, redBangs);
                }
                
            }
        }
        
        if (showMenu) {
            renderMainMenu(renderer);
        } 
        if(showAvatarMaker){
            renderAvatarMaker(renderer, selectedCategory, playerFaceTexture);
        }

        
        while (Helper::SDL_PollEvent498(&e)) {

            if (e.type == SDL_QUIT) {
                running = false;
                Helper::SDL_RenderPresent498(renderer);
                exit(0);
            }
  
            if (e.type == SDL_KEYDOWN) {
                // Check for avatar maker activation or other functionalities
                if (e.key.keysym.sym == SDLK_1) {
                    selectedCategory = 0; // select 'base' category
                } else if (e.key.keysym.sym == SDLK_2) {
                    selectedCategory = 1; // select 'hair' category
                } else if (e.key.keysym.sym == SDLK_3) {
                    selectedCategory = 2; // select 'bangs' category
                } else if (e.key.keysym.sym == SDLK_4) {
                    selectedCategory = 3; // select 'eyes' category
                } else if (e.key.keysym.sym == SDLK_5) {
                    selectedCategory = 4; // select 'mouth' category
                } else if (e.key.keysym.sym == SDLK_6) {
                    selectedCategory = 5; // select 'outfit' category
                } else if (e.key.keysym.sym == SDLK_LEFT && showAvatarMaker) {
                    if (selectedCategory != -1) {  // move left in the current category
                        changePart(selectedCategory, -1);
                    }
                } else if (e.key.keysym.sym == SDLK_RIGHT && showAvatarMaker) {
                    if (selectedCategory != -1) {  // move right in the current category
                        changePart(selectedCategory, 1);
                    }
                }else if (e.key.keysym.sym == SDLK_7 && showAvatarMaker) {
                    if(gameConfig.cameraOn) gameConfig.cameraOn = false;
                    else gameConfig.cameraOn = true;
                } else if (e.key.keysym.sym == SDLK_a && !showAvatarMaker) {
                    showMenu = false;
                    showAvatarMaker = true;  // show avatar maker
                } else if (e.key.keysym.sym == SDLK_s) {
                    showMenu = false;
                    showAvatarMaker = false;
                } else if (e.key.keysym.sym == SDLK_SPACE || e.key.keysym.sym == SDLK_RETURN) {
                    advanceIntro = true;
                } else if (e.key.keysym.sym == SDLK_ESCAPE) {
                    showMenu = true;  // return to main menu
                    showAvatarMaker = false;
                    selectedCategory = -1;
                }
            
            }
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                advanceIntro = true;
            }
        }
        
        // Player movement
        if(!showMenu && !showAvatarMaker){
            if (state[SDL_SCANCODE_UP]) {
                movement_direction.y -= gameConfig.player_movement_speed * 100;
            }
            if (state[SDL_SCANCODE_DOWN]) {
                movement_direction.y += gameConfig.player_movement_speed * 100;
            }
            if (state[SDL_SCANCODE_LEFT]) {
                movement_direction.x -= gameConfig.player_movement_speed * 100;
            }
            if (state[SDL_SCANCODE_RIGHT]) {
                movement_direction.x += gameConfig.player_movement_speed * 100;
            }
            
            if (glm::length(movement_direction) > 0) {
                movement_direction = glm::normalize(movement_direction) * player_movement_speed;
                glm::vec2 proposed_position = player_position + movement_direction;
                player_position = proposed_position;
            }
            
            gameConfig.player_position_x = player_position.x;
            gameConfig.player_position_y = player_position.y;
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
            if(showMenu == false && showAvatarMaker == false)
            Helper::SDL_RenderPresent498(renderer);
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
            
            
            // Check for blocking actors
            bool blocked = false;
            for (const auto& actor : actors_custom) {
                if (actor.blocking && actor.transform_position.x == player_position.x && actor.transform_position.y == player_position.y) {
                    blocked = true;
                    break;
                }
                
            }
            
            if (!blocked) {
                if(gameConfig.needsSorting == true){
                    sortActorsForRendering();
                    gameConfig.needsSorting = false;
                }
                gameConfig.player_position_x = player_position.x;
                gameConfig.player_position_y = player_position.y;
                gameConfig.needsSorting = true;
            }
            
            
            player_position = {gameConfig.player_position_x, gameConfig.player_position_y};
        
        if (introDone && showMenu == false && showAvatarMaker == false){
                updateNPCPositions();
            
        }

            if (!renderMapAroundPlayer(player_position, renderer, playerFaceTexture)) {
                movedOnFromInit = true;
                
            }
            else{

                if(introDone && !showMenu && !showAvatarMaker){
                    gameConfig.cameraPosition = glm::mix(gameConfig.cameraPosition, player_position, gameConfig.cam_ease_factor);
                     Helper::SDL_RenderPresent498(renderer);
                }
                
                

            }

            
    }
        
    
    
    return 0;
    
}





