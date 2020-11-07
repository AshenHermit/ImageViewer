#include <iostream>
#include <algorithm>
#include <cctype>
#include <string>
#include "SDL/SDL.h"
#include "SDL/SDL_image.h"
#include "CEV/CEV_gif.h"
#include <experimental/filesystem>

#if       _WIN32_WINNT < 0x0500
#undef  _WIN32_WINNT
#define _WIN32_WINNT   0x0500
#endif
#include <windows.h>
#include <vector>

#define CONSOLE_LOG(val) std::cout<<val<<std::endl

const int SCREEN_FPS = 60;
const int SCREEN_TICKS_PER_FRAME = 1000 / SCREEN_FPS;

namespace fs = std::experimental::filesystem;

template<typename T>
struct Vector2 {
	Vector2() {}
	Vector2(T _x, T _y) {
		x = _x;
		y = _y;
	}
	T x = 0;
	T y = 0;
};

typedef Vector2<int> Vectori;
typedef Vector2<float> Vectorf;

int winW = 1324;
int winH = 917;

bool isLeftDown = false;

Vectori mousePos;

Vectorf offset;
Vectorf offsetTarget;

Vectorf dragStartPos;
Vectorf dragOffset;

float scale = 1.0F;
float scaleTarget = 1.0F;

SDL_Rect srcrect;
SDL_Rect dstrect;
SDL_Rect viewport;

Vectorf framePoints[4];


SDL_Renderer* renderer = NULL;
SDL_Texture* imgTex = NULL;
CEV_GifAnim* gifAnim = NULL;

std::string filePath;
std::string fileFormat;
std::string folderPath;

std::vector<std::string> fileList;
int currentFileId = 0;

bool isGif = false;
bool isInFullscreen = false;

void updateRects() {
	framePoints[0].x = -(srcrect.w / 2.0F - offset.x) * scale; framePoints[0].y = -(srcrect.h / 2.0F - offset.y) * scale;
	framePoints[1].x = (srcrect.w / 2.0F + offset.x) * scale; framePoints[1].y = -(srcrect.h / 2.0F - offset.y) * scale;
	framePoints[2].x = (srcrect.w / 2.0F + offset.x) * scale; framePoints[2].y = (srcrect.h / 2.0F + offset.y) * scale;
	framePoints[3].x = -(srcrect.w / 2.0F - offset.x) * scale; framePoints[2].y = (srcrect.h / 2.0F + offset.y) * scale;

	dstrect.x = framePoints[0].x + (float)winW / 2.0F;
	dstrect.y = framePoints[0].y + (float)winH / 2.0F;
	dstrect.w = framePoints[2].x - framePoints[0].x;
	dstrect.h = framePoints[2].y - framePoints[0].y;

	//dstrect.x = offset.x - (float)winW / 2.0F * scale;
	//dstrect.y = offset.y - (float)winH / 2.0F * scale;
	//dstrect.w = srcrect.w * scale;
	//dstrect.h = srcrect.h * scale;

	viewport.x = offset.x - winW / 2.0F * scale;
	viewport.y = offset.y - winH / 2.0F * scale;
	viewport.w = (float)winW * scale;
	viewport.h = (float)winH * scale;
}

int FPS = 50;    // Assign a FPS
int NextTick, interval;
int aTick = 0;

// Initialize FPS_Fn( )
void FPS_Initial(void) {
	NextTick = 0;
	interval = 1 * 1000 / FPS;
	return;
}

// Frame Per Second Function  , put this in a loop
void FPS_Fn(void) {
	if (NextTick > SDL_GetTicks()) SDL_Delay(NextTick - SDL_GetTicks());
	NextTick = SDL_GetTicks() + interval;
	return;
}

void toLower(std::string& str) {
	std::transform(str.begin(), str.end(), str.begin(),
		[](unsigned char c) { return std::tolower(c); });
}

bool validFormat(std::string path, std::string* format = nullptr) {
	path = path.substr(path.find_last_of(".")+1);
	toLower(path);
	if (path == "png") {
		if (format != nullptr)* format = "png";
		return true;
	}else 
	if (path == "jpeg" || path == "jpg") {
		if (format != nullptr)*format = "jpg";
		return true;
	}else
	if (path == "gif") {
		if (format != nullptr) *format = "gif";
		return true;
	}else
	if (path == "tiff") {
		if (format != nullptr) *format = "tiff";
		return true;
	}else
	if (path == "webp") {
		if (format != nullptr) *format = "webp";
		return true;
	}
	
	return false;
}

std::string cp1251_to_utf8(const char* str) {
	std::string res;
	int result_u, result_c;
	result_u = MultiByteToWideChar(1251, 0, str, -1, 0, 0);
	if (!result_u) { return 0; }
	wchar_t* ures = new wchar_t[result_u];
	if (!MultiByteToWideChar(1251, 0, str, -1, ures, result_u)) {
		delete[] ures;
		return 0;
	}
	result_c = WideCharToMultiByte(65001, 0, ures, -1, 0, 0, 0, 0);
	if (!result_c) {
		delete[] ures;
		return 0;
	}
	char* cres = new char[result_c];
	if (!WideCharToMultiByte(65001, 0, ures, -1, cres, result_c, 0, 0)) {
		delete[] cres;
		return 0;
	}
	delete[] ures;
	res.append(cres);
	delete[] cres;
	return res;
}

void scanDirectory() {
	fileList.clear();
	for (auto& p : fs::directory_iterator(fs::u8path(folderPath)))
	{
		if (p.status().type() == fs::file_type::regular) {
			if (validFormat(p.path().string())) {
				if (p.path().string() == filePath) currentFileId = fileList.size();
				fileList.push_back(cp1251_to_utf8(p.path().string().c_str()));
			}
		}
	}
}

std::string toUtf8(const std::wstring& str)
{
	std::string ret;
	int len = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), str.length(), NULL, 0, NULL, NULL);
	if (len > 0)
	{
		ret.resize(len);
		WideCharToMultiByte(CP_UTF8, 0, str.c_str(), str.length(), &ret[0], len, NULL, NULL);
	}
	return ret;
}

void openImage(std::string filename, bool checkDir) {
	// zoom = 1.0F;
	if (validFormat(filename, &fileFormat)) {
		CONSOLE_LOG(filename);
		filePath = filename;
		isGif = (fileFormat == "gif");

		if (imgTex != NULL) SDL_DestroyTexture(imgTex);

		if (checkDir) {
			folderPath = filename.substr(0, filename.find_last_of("\\") + 1);
			scanDirectory();
		}

		if (isGif) {
			if (gifAnim != NULL) CEV_gifAnimFree(gifAnim);
			gifAnim = CEV_gifAnimLoad(filename.c_str(), renderer);
			imgTex = CEV_gifTexture(gifAnim);
			CEV_gifLoopMode(gifAnim, GIF_MODE::GIF_REPEAT_FOR);
		}
		else {
			imgTex = IMG_LoadTexture(renderer, filename.c_str());
			std::cout << filename << std::endl;
		}

		SDL_QueryTexture(imgTex, NULL, NULL, &srcrect.w, &srcrect.h);
	}
}

int main(int argc, char *argv[])
{

	ShowWindow(GetConsoleWindow(), SW_HIDE);

	for (int i = 0; i < argc; i++)
	{
		std::cout << argv[i] << std::endl;
	}

	SDL_bool done;
	SDL_Window* window;
	SDL_Event event;                        // Declare event handle        

	SDL_Init(SDL_INIT_VIDEO);               // SDL2 initialization
	IMG_Init(IMG_INIT_PNG);

	window = SDL_CreateWindow(  // Create a window
		"Image Viewer",
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		winW,
		winH,
		SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
	);

	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	SDL_SetRenderDrawColor(renderer, 32, 32, 32, 255);

	// Check that the window was successfully made
	if (window == NULL) {
		// In the event that the window could not be made...
		SDL_Log("Could not create window: %s", SDL_GetError());
		SDL_Quit();
		return 1;
	}

	SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

	if (argc > 1) {
		openImage(argv[1], true);
	}

	FPS_Initial();

	done = SDL_FALSE;
	while (!done) {// Program loop
		// aTick++; FPS_Fn();
		while (!done && SDL_PollEvent(&event)) {
			switch (event.type) {
				case (SDL_QUIT): {// In case of exit
					done = SDL_TRUE;
					break;
				}

				case (SDL_DROPFILE): {// In case if dropped file
					openImage(event.drop.file, true);
					SDL_free(event.drop.file);// Free dropped_filedir memory
					break;
				}

				case (SDL_WINDOWEVENT): {
					if (event.window.event == SDL_WINDOWEVENT_RESIZED 
						|| event.window.event == SDL_WINDOWEVENT_MAXIMIZED
						|| event.window.event == SDL_WINDOWEVENT_MINIMIZED) {
						SDL_GetWindowSize(window, &winW, &winH);
					}

					break;
				}

				case(SDL_MOUSEWHEEL): {
					float zoom = std::exp(event.wheel.y / 10.0F);
					float x = (mousePos.x - (winW / 2.0F)) - offsetTarget.x;
					float y = (mousePos.y - (winH / 2.0F)) - offsetTarget.y;
					//offsetTarget.x -= x / (scaleTarget * zoom) - x / scaleTarget;
					//offsetTarget.y -= y / (scaleTarget * zoom) - y / scaleTarget;
					//offsetTarget.x -= (offsetTarget.x) / (scaleTarget * zoom) - (offsetTarget.x) / scaleTarget;
					//offsetTarget.y -= (offsetTarget.y) / (scaleTarget * zoom) - (offsetTarget.y) / scaleTarget;
					scaleTarget *= zoom;
					break;
				}
				
				case(SDL_MOUSEMOTION): {
					SDL_GetMouseState(&mousePos.x, &mousePos.y);
					if (isLeftDown) {
						offsetTarget.x = dragOffset.x + (mousePos.x - dragStartPos.x)/ scale;
						offsetTarget.y = dragOffset.y + (mousePos.y - dragStartPos.y)/ scale;
					}
					break;
				}

				case(SDL_MOUSEBUTTONDOWN): {
					if (event.button.button == SDL_BUTTON_LEFT) {
						dragOffset.x = offsetTarget.x;
						dragOffset.y = offsetTarget.y;
						dragStartPos.x = mousePos.x;
						dragStartPos.y = mousePos.y;
						isLeftDown = true;
					}
					break;
				}

				case(SDL_MOUSEBUTTONUP): {
					if (event.button.button == SDL_BUTTON_LEFT) {
						isLeftDown = false;
					}
					break;
				}

				case(SDL_KEYDOWN): {
					if (event.key.keysym.sym == SDLK_ESCAPE) {
						done = SDL_TRUE;
					}else
					if (event.key.keysym.sym == SDLK_LEFT || event.key.keysym.sym == SDLK_a) {
						currentFileId -= 1;
						if (currentFileId < 0) currentFileId = fileList.size() - 1;
						openImage(fileList[currentFileId], false);
					}
					else
					if (event.key.keysym.sym == SDLK_RIGHT || event.key.keysym.sym == SDLK_d) {
						currentFileId += 1;
						if (currentFileId >= fileList.size()) currentFileId = 0;
						openImage(fileList[currentFileId], false);
					}else
					if (event.key.keysym.sym == SDLK_F11) {
						isInFullscreen = !isInFullscreen;
						if(isInFullscreen) SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
						else SDL_SetWindowFullscreen(window, 0);
					}else
					if (event.key.keysym.sym == SDLK_F5) {
						openImage(fileList[currentFileId], false);
					}
					break;
				}
			}
		}

		scale += (scaleTarget - scale) / 4.0F;
		offset.x += (offsetTarget.x - offset.x) / 4.0F;
		offset.y += (offsetTarget.y - offset.y) / 4.0F;

		if (isGif) {
			CEV_gifAnimAuto(gifAnim);
		}

		updateRects();

		SDL_RenderClear(renderer);

		if (imgTex != NULL) SDL_RenderCopy(renderer, imgTex, NULL, &dstrect);

		SDL_RenderPresent(renderer);
	}

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);        // Close and destroy the window
	SDL_DestroyTexture(imgTex);

	IMG_Quit();
	SDL_Quit();                       // Clean up

	return 0;
}