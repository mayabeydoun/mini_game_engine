

#include <stdio.h>
#define _SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <filesystem>
#include <cstdlib> // For exit
#include <cstdio>
#include <vector>
#include <map>
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


#include <opencv2/opencv.hpp>
#include <opencv2/objdetect.hpp>


cv::VideoCapture initializeCamera() {
    cv::VideoCapture cap(1); // open the camera
    if (!cap.isOpened()) {
        std::cerr << "Failed to open camera" << std::endl;
        exit(1);
    }
    return cap;
}

cv::Mat captureFrame(cv::VideoCapture& cap) {
    cv::Mat frame;
    cap >> frame;
    if (frame.empty()) {
        std::cerr << "Failed to capture frame" << std::endl;
        exit(1);
    }
    
    return frame;
}

void detectFaces(cv::Mat& frame, std::vector<cv::Rect>& faces, cv::CascadeClassifier& face_cascade) {
    cv::Mat frame_gray;
    cvtColor(frame, frame_gray, cv::COLOR_BGR2GRAY); // Convert to grayscale
    equalizeHist(frame_gray, frame_gray); // Improve contrast
    
    
    double scaleFactor = 1.2;
    int minNeighbors = 3;
    cv::Size minSize(200,200);
    cv::Size maxSize(400, 400);

    face_cascade.detectMultiScale(frame_gray, faces, scaleFactor, minNeighbors, 0 | cv::CASCADE_SCALE_IMAGE, minSize, maxSize);
}


cv::CascadeClassifier loadFaceCascade(const std::string& cascadePath) {
    cv::CascadeClassifier face_cascade;
    if (!face_cascade.load(cascadePath)) {
        std::cerr << "Error loading face cascade" << std::endl;
        exit(1);
    }
    return face_cascade;
}

void swapRedBlueChannels(cv::Mat& image) {
    CV_Assert(image.type() == CV_8UC4);

    std::vector<cv::Mat> channels;
    cv::split(image, channels);

    std::swap(channels[0], channels[2]);

    cv::merge(channels, image);
}


SDL_Texture* MatToTexture(SDL_Renderer* renderer, const cv::Mat& mat) {
    SDL_Texture* tex = nullptr;
    
    
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    if (!mat.empty()) {
        tex = SDL_CreateTexture(renderer,
                                SDL_PIXELFORMAT_RGBA32,
                                SDL_TEXTUREACCESS_STREAMING,
                                mat.cols,
                                mat.rows);

        
        if (mat.step > static_cast<size_t>(std::numeric_limits<int>::max())) {
            std::cerr << "Stride exceeds the maximum value for an int, texture may not render correctly." << std::endl;
        } else {
            SDL_UpdateTexture(tex, nullptr, mat.data, static_cast<int>(mat.step));
        }
    }
    if (tex == nullptr) {
        std::cout << "Failed to create texture" << std::endl;
        exit(1);
    } else {
        std::cout << "Texture created successfully" << std::endl;
    }
    
    return tex;
}


SDL_Texture* processFrame(cv::Mat& frame, const std::vector<cv::Rect>& faces, SDL_Renderer* renderer, const std::string& overlayPath) {
    SDL_Texture* faceTexture = nullptr;
    if (!faces.empty()) {
        cv::Rect faceRect = faces[0];
        cv::Mat faceROI = frame(faceRect);

        if (faceRect.width > 0 && faceRect.height > 0) {
            cv::Mat convertedROI;
            cv::cvtColor(faceROI, convertedROI, cv::COLOR_BGR2RGBA); // Convert to RGBA
            swapRedBlueChannels(convertedROI);  // Swap the red and blue channels manually

            // Use mask to blend onto a transparent background
            cv::Mat mask(faceROI.size(), CV_8UC1, cv::Scalar(0));
            cv::ellipse(mask, cv::Point(faceRect.width / 2, faceRect.height / 2),
                        cv::Size(faceRect.width / 2.5, faceRect.height / 2), 0, 0, 360, cv::Scalar(255), -1);
            cv::Mat blendedFace(convertedROI.size(), CV_8UC4, cv::Scalar(0, 0, 0, 0));
            convertedROI.copyTo(blendedFace, mask);

            // Save and load the texture
            cv::imwrite("blendedFace.png", blendedFace);
            faceTexture = IMG_LoadTexture(renderer, "blendedFace.png");
        }
    }
    return faceTexture;
}
