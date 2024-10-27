/**
* Author: Jia Huang
* Assignment: Lunar Lander
* Date due: 2024-10-27, 11:59pm
* I pledge that I have completed this assignment without
* collaborating with anyone else, in conformance with the
* NYU School of Engineering Policies and Procedures on
* Academic Misconduct.
**/
#define LOG(argument) std::cout << argument << '\n'
#define STB_IMAGE_IMPLEMENTATION
#define GL_SILENCE_DEPRECATION
#define GL_GLEXT_PROTOTYPES 1
#define FIXED_TIMESTEP 0.0166666f
#define PLATFORM_COUNT 22

#ifdef _WINDOWS
    #include <GL/glew.h>
#endif

#include <SDL.h>
#include <SDL_opengl.h>
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "ShaderProgram.h"
#include "stb_image.h"
#include "Entity.h"
#include <vector>
#include <ctime>
#include "cmath"

// ————— CONSTANTS ————— //
constexpr int WINDOW_WIDTH  = 640 * 2,
              WINDOW_HEIGHT = 480 * 2;

constexpr float BG_RED     = 0.9765625f,
                BG_GREEN   = 0.97265625f,
                BG_BLUE    = 0.9609375f,
                BG_OPACITY = 1.0f;

constexpr int VIEWPORT_X = 0,
              VIEWPORT_Y = 0,
              VIEWPORT_WIDTH  = WINDOW_WIDTH,
              VIEWPORT_HEIGHT = WINDOW_HEIGHT;

constexpr char V_SHADER_PATH[] = "shaders/vertex_textured.glsl",
               F_SHADER_PATH[] = "shaders/fragment_textured.glsl";

constexpr float MILLISECONDS_IN_SECOND = 1000.0;
constexpr char KIRBY_FILEPATH[] = "assets/swim_kirby.png";
constexpr char TERRAIN_FILEPATH[] = "assets/terrain.png";
constexpr char LAVA_FILEPATH[] = "assets/lava.png";
constexpr char GOAL_FILEPATH[] = "assets/goal.png";
constexpr char MISSION_FAILED_FILEPATH[] = "assets/mission_failed.png";
constexpr char MISSION_ACCOMPLISHED_FILEPATH[] = "assets/mission_accomplished.png";
constexpr char BACKGROUND_FILEPATH[] = "assets/background.jpg";

constexpr GLint NUMBER_OF_TEXTURES = 1,
                LEVEL_OF_DETAIL    = 0,
                TEXTURE_BORDER     = 0;

// ————— STRUCTS AND ENUMS —————//
enum AppStatus  { RUNNING, TERMINATED };
enum FilterType { NEAREST, LINEAR     };

struct GameState
{
    Entity* player;
    Entity* platforms;
    Entity* non_collidables;
};

// ————— VARIABLES ————— //
GameState g_game_state;

SDL_Window* g_display_window;
AppStatus g_app_status = RUNNING;

ShaderProgram g_shader_program;
glm::mat4 g_view_matrix, g_projection_matrix;

float g_previous_ticks = 0.0f;
float g_accumulator = 0.0f;

void initialise();
void process_input();
void update();
void render();
void shutdown();

GLuint load_texture(const char* filepath);

// ———— GENERAL FUNCTIONS ———— //
GLuint load_texture(const char* filepath, FilterType filterType)
{
    int width, height, number_of_components;
    unsigned char* image = stbi_load(filepath, &width, &height, &number_of_components, STBI_rgb_alpha);
    
    if (image == NULL)
    {
        LOG("Unable to load image. Make sure the path is correct.");
        assert(false);
    }
    
    GLuint textureID;
    glGenTextures(NUMBER_OF_TEXTURES, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, LEVEL_OF_DETAIL, GL_RGBA, width, height, TEXTURE_BORDER,
                 GL_RGBA, GL_UNSIGNED_BYTE, image);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    filterType == NEAREST ? GL_NEAREST : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                    filterType == NEAREST ? GL_NEAREST : GL_LINEAR);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    stbi_image_free(image);
    
    return textureID;
}

void initialise()
{
    SDL_Init(SDL_INIT_VIDEO);
    g_display_window = SDL_CreateWindow("Kirby Dive!",
                                      SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                      WINDOW_WIDTH, WINDOW_HEIGHT,
                                      SDL_WINDOW_OPENGL);
    
    SDL_GLContext context = SDL_GL_CreateContext(g_display_window);
    SDL_GL_MakeCurrent(g_display_window, context);
    
    if (g_display_window == nullptr)
    {
        std::cerr << "Error: SDL window could not be created.\n";
        shutdown();
    }
    
#ifdef _WINDOWS
    glewInit();
#endif
    
    glViewport(VIEWPORT_X, VIEWPORT_Y, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
    
    g_shader_program.load(V_SHADER_PATH, F_SHADER_PATH);
    
    g_view_matrix       = glm::mat4(1.0f);
    g_projection_matrix = glm::ortho(-5.0f, 5.0f, -3.75f, 3.75f, -1.0f, 1.0f);
    
    g_shader_program.set_projection_matrix(g_projection_matrix);
    g_shader_program.set_view_matrix(g_view_matrix);
    
    glUseProgram(g_shader_program.get_program_id());
    
    glClearColor(BG_RED, BG_BLUE, BG_GREEN, BG_OPACITY);
    
    // ————— PLAYER ————— //
    GLuint player_texture_id = load_texture(KIRBY_FILEPATH, NEAREST);
    GLuint terrain_texture_id = load_texture(TERRAIN_FILEPATH, NEAREST);
    GLuint lava_texture_id = load_texture(LAVA_FILEPATH, NEAREST);
    GLuint goal_texture_id = load_texture(GOAL_FILEPATH, NEAREST);
    GLuint mission_failed_texture_id = load_texture(MISSION_FAILED_FILEPATH, NEAREST);
    GLuint mission_accomplished_texture_id = load_texture(MISSION_ACCOMPLISHED_FILEPATH, NEAREST);
    GLuint background_texture_id = load_texture(BACKGROUND_FILEPATH, NEAREST);
    
    int player_animation[3][12] = {
        { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 },
        { 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23 },
        { 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35 },
    };
    
    g_game_state.player = new Entity(
        player_texture_id,         // texture id
        1.0f,                      // speed
        player_animation,  // animation index sets
        0.0f,                      // animation time
        12,                         // animation frame amount
        0,                         // current animation index
        12,                        // animation column amount
        3,                          // animation row amount
        1.0f,                      // width
        1.0f,                       // height
        PLAYER
    );
    
    g_game_state.player->fall_down();
    g_game_state.player->set_position(glm::vec3(0.0f, 2.0f, 0.0f));
    
    g_game_state.platforms = new Entity[PLATFORM_COUNT];
    
    GLuint platform_texture_id;
    EntityType platform_type;

    // Set the type of every platform entity to PLATFORM
    for (int i = 0; i < PLATFORM_COUNT; i++)
    {
        if ((i > 2 && i < 4) || (i > 15 && i < 20)) {
            platform_texture_id = goal_texture_id;
            platform_type = GOAL;
        }
        else {
            platform_texture_id = lava_texture_id;
            platform_type = LAVA;
        }
    
        g_game_state.platforms[i].set_texture_id(platform_texture_id);
        g_game_state.platforms[i].set_position(glm::vec3(5.5f - PLATFORM_COUNT / 2.0 + i / 2.0, -2.95f, 0.0f));
        g_game_state.platforms[i].set_width(0.05f);
        g_game_state.platforms[i].set_height(0.05f);
        g_game_state.platforms[i].set_entity_type(platform_type);
        g_game_state.platforms[i].set_scale(glm::vec3(0.5f));
        g_game_state.platforms[i].update(0.0f, NULL, NULL, 0);
    }
    
    g_game_state.non_collidables = new Entity[3];

    g_game_state.non_collidables[0].set_texture_id(mission_failed_texture_id);
    g_game_state.non_collidables[0].set_position(glm::vec3(0.0f, 1.0f, 0.0f));
    g_game_state.non_collidables[0].set_width(0.05f);
    g_game_state.non_collidables[0].set_height(0.05f);
    g_game_state.non_collidables[0].set_entity_type(MESSAGE);
    g_game_state.non_collidables[0].set_scale(glm::vec3(4.0f, 2.5f, 1.0f));
    g_game_state.non_collidables[0].update(0.0f, NULL, NULL, 0);
    
    g_game_state.non_collidables[1].set_texture_id(mission_accomplished_texture_id);
    g_game_state.non_collidables[1].set_position(glm::vec3(0.0f, 1.0f, 0.0f));
    g_game_state.non_collidables[1].set_width(0.05f);
    g_game_state.non_collidables[1].set_height(0.05f);
    g_game_state.non_collidables[1].set_entity_type(MESSAGE);
    g_game_state.non_collidables[1].set_scale(glm::vec3(4.0f, 2.5f, 1.0f));
    g_game_state.non_collidables[1].update(0.0f, NULL, NULL, 0);
    
    g_game_state.non_collidables[2].set_texture_id(background_texture_id);
    g_game_state.non_collidables[2].set_position(glm::vec3(0.0f));
    g_game_state.non_collidables[2].set_width(0.05f);
    g_game_state.non_collidables[2].set_height(0.05f);
    g_game_state.non_collidables[2].set_entity_type(MESSAGE);
    g_game_state.non_collidables[2].set_scale(glm::vec3(10.0f, 8.0f, 1.0f));
    g_game_state.non_collidables[2].update(0.0f, NULL, NULL, 0);
    
    // ————— GENERAL ————— //
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void process_input()
{
    // VERY IMPORTANT: If nothing is pressed, we don't want to go anywhere
    g_game_state.player->set_movement(glm::vec3(0.0f));
    
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type) {
            case SDL_QUIT:
            case SDL_WINDOWEVENT_CLOSE:
                g_app_status = TERMINATED;
                break;
                
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_q: g_app_status = TERMINATED;
                    default:     break;
                }
                
            default:
                break;
        }
    }
    
    if (g_game_state.player->get_winner() || g_game_state.player->get_loser()) return;
    
    const Uint8 *key_state = SDL_GetKeyboardState(NULL);
    
    if (key_state[SDL_SCANCODE_UP])
    {
        g_game_state.player->swim_up();
    }
    else
    {
        g_game_state.player->fall_down();
    }

    if (key_state[SDL_SCANCODE_LEFT])
    {
        g_game_state.player->turn_left();
    }
    else if (key_state[SDL_SCANCODE_RIGHT])
    {
        g_game_state.player->turn_right();
    }
    
    if (glm::length(g_game_state.player->get_movement()) > 1.0f)
        g_game_state.player->normalise_movement();
}


void update()
{
    float ticks = (float)SDL_GetTicks() / MILLISECONDS_IN_SECOND;
    float delta_time = ticks - g_previous_ticks;
    g_previous_ticks = ticks;

    delta_time += g_accumulator;

    if (delta_time < FIXED_TIMESTEP)
    {
        g_accumulator = delta_time;
        return;
    }

    while (delta_time >= FIXED_TIMESTEP)
    {
        g_game_state.player->update(FIXED_TIMESTEP, NULL, g_game_state.platforms, PLATFORM_COUNT);
        delta_time -= FIXED_TIMESTEP;
    }

    g_accumulator = delta_time;
}


void render()
{
    glClear(GL_COLOR_BUFFER_BIT);
    
    g_game_state.non_collidables[2].render(&g_shader_program);
    
    g_game_state.player->render(&g_shader_program);
    for (int i = 0; i < PLATFORM_COUNT; i++) {
        g_game_state.platforms[i].render(&g_shader_program);
    }
    
    if (g_game_state.player->get_winner()) g_game_state.non_collidables[1].render(&g_shader_program);
    if (g_game_state.player->get_loser()) g_game_state.non_collidables[0].render(&g_shader_program);
    
    SDL_GL_SwapWindow(g_display_window);
}


void shutdown()
{
    SDL_Quit();
    delete [] g_game_state.non_collidables;
    delete [] g_game_state.platforms;
    delete g_game_state.player;
}


int main(int argc, char* argv[])
{
    initialise();
    
    while (g_app_status == RUNNING)
    {
        process_input();
        update();
        render();
    }
    
    shutdown();
    return 0;
}
