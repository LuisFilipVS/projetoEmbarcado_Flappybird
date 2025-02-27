#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#include <stdlib.h>
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "ws2812b.pio.h"

//Definição das portas GPIO para o botão A e LED RGB
#define BUTTON_PIN 5
#define LED_R 13
#define LED_G 11
#define LED_B 12

//Definição do display OLED
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C

#define cor true

//Definição das configurações para display OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define PIPE_WIDTH 10   // Largura do obstáculo
#define PIPE_GAP 20     // Distância entre os dois obstáculos
#define PIPE_SPACING 64 // Distância entre os canos
#define OBSTACLES 2     // Quantidade de Obstáculos na tela

#define LED_COUNT 25
#define MATRIZ_LED_PIN 7

// Estrutura usada para auxiliar na definição das cores que serão destacadas no display LED
typedef struct{
    uint8_t R;
    uint8_t G;
    uint8_t B;
} led;

led MATRIZ_LED[LED_COUNT];

PIO pio = pio0;
uint offset;
uint sm;

// Estrutura da representação do pássaro
typedef struct {
    int x, y;
    int velocity;
} Bird;

// Estrutura da representação da estrutura
typedef struct {
    int x, height;
} Pipe;

//Instanciação dos principais elementos do sistema.
Bird bird;
Pipe pipes[OBSTACLES];
ssd1306_t display;

//Configuração para iniciar a Placa em modo Bootsel ao selecionar o botão B
#include "pico/bootrom.h"
#define botaoB 6
void gpio_irq_handler(uint gpio, uint32_t events){
    if(gpio == botaoB){
        reset_usb_boot(0, 0);
    } 
}

//Array que reprensenta cada numero na matriz de led 5x5
const uint8_t numbers_indices[10][25] = {
    {23,22,21,18,11,8,1,16,13,6,3,2},    // Número 0
    {22,17,12,7,2},                      // Número 1
    {23,22,21,18,11,12,13,6,3,2,1},      // Número 2
    {23,22,21,18,11,12,13,8,3,2,1},      // Número 3
    {23,21,16,18,13,12,11,8,1},          // Número 4
    {23,22,21,16,13,12,11,8,3,2,1},      // Número 5
    {23,22,21,16,13,12,11,8,6,3,2,1},    // Número 6
    {23,22,21,18,11,8,1},                // Número 7
    {23,22,21,18,11,8,1,16,13,12,6,3,2}, // Número 8
    {23,22,21,18,11,8,1,16,13,12}        // Número 9
};

led matriz_led[LED_COUNT];
static int INCREMENT = 0;

// Função que junta 3 bytes com informações de cores e mais 1 byte vazio para temporização
uint32_t valor_rgb(uint8_t B, uint8_t R, uint8_t G){
    return (G << 24) | (R << 16) | (B << 8);
  };

  //Usado para configurar cada led dentro da matriz de led.
void set_led(uint8_t indice, uint8_t r, uint8_t g, uint8_t b){
    if(indice < 25){
    matriz_led[indice].R = r;
    matriz_led[indice].G = g;
    matriz_led[indice].B = b;
    }
};


//Função recebe um numero de 0 a 9 e configura o mesmo para ser mostrado na matriz de led. 
void config_number_led(int number){
    int size = sizeof(numbers_indices[number]) / sizeof(numbers_indices[number][0]);

    for(int i = 0; i < size; i++ ){
        if(numbers_indices[number][i] == 0){
            break;
        }
        set_led(numbers_indices[number][i],0,1,0);
    }
};

//Função usada para desligar todos os leds
void clear_leds(){
    for(uint8_t i = 0; i < LED_COUNT; i++){
        matriz_led[i].R = 0;
        matriz_led[i].B = 0;
        matriz_led[i].G = 0;
    }
};

// Função que envia os dados do array para os leds via PIO
void print_leds(PIO pio, uint sm){
    uint32_t valor;
    for(uint8_t i = 0; i < LED_COUNT; i++){
        valor = valor_rgb(matriz_led[i].B, matriz_led[i].R,matriz_led[i].G);
        pio_sm_put_blocking(pio, sm, valor);
    }
};

//Função usada para incrementar ou decrementar o número do LED baseado no clique dos botões
void update_number_led(int number, PIO pio, uint sm){
    clear_leds();
    printf("%d", number);
    
    if (number == 1){
        INCREMENT += 1;
    }

    if (number == 0){
        INCREMENT -= 1;
    }

    if(INCREMENT < 0){
        INCREMENT = 0;
    }
    
    int var = INCREMENT % 10;
    config_number_led(var);
    print_leds(pio,sm);
}

//Função usada para iniciar todos os recurso de Hardware
void init_hardware() {
    //Iniciar todas as portas de entrada e saída
    stdio_init_all();
    
    //Botão A
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

    //Leds RGB
    gpio_init(LED_R);
    gpio_set_dir(LED_R, GPIO_OUT);
    gpio_init(LED_G);
    gpio_set_dir(LED_G, GPIO_OUT);
    gpio_init(LED_B);
    gpio_set_dir(LED_B, GPIO_OUT);

    //Display OLED
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SCL);
    gpio_pull_up(I2C_SDA);

    //Inicialização do display OLED
    ssd1306_init(&display, SCREEN_WIDTH, SCREEN_HEIGHT, false, endereco, I2C_PORT);
    ssd1306_config(&display);
    ssd1306_send_data(&display);
    ssd1306_fill(&display, false);
    
    //Botão B (Usado para reiniciar a placa em modo BOOSTSEL)
    gpio_init(botaoB);
    gpio_set_dir(botaoB, GPIO_IN);
    gpio_pull_up(botaoB);

    //Configura a matriz de LED
    pio = pio0; 
    bool ok = set_sys_clock_khz(128000, false);
    offset = pio_add_program(pio, &ws2812b_program);
    sm = pio_claim_unused_sm(pio, true);
    ws2812b_program_init(pio, sm, offset, MATRIZ_LED_PIN);

    config_number_led(0);
    print_leds(pio,sm);

}

//Função usada para desenhar o passaro na tela
void draw_bird() {
    ssd1306_rect(&display, bird.y, bird.x, 5, 5, 1, true);
}

//Função utilizada para reinicializar o jogo
void reset_game() {
    bird.y = SCREEN_HEIGHT / 2;
    bird.velocity = 0;
    INCREMENT = 0;
    
    for (int i = 0; i < OBSTACLES; i++) {
        pipes[i].x = SCREEN_WIDTH + PIPE_SPACING * i;
        pipes[i].height = (rand() % (SCREEN_HEIGHT - PIPE_GAP - 10)) + 5;
    }

    ssd1306_fill(&display, false);
    ssd1306_send_data(&display);
}

//Função utilizada para desenhar os obstáculos na tela na posições definidas
void draw_pipes() {
    for (int i = 0; i < OBSTACLES; i++) {
        ssd1306_rect(&display, 0                          ,pipes[i].x, PIPE_WIDTH, pipes[i].height, 1, true);
        ssd1306_rect(&display, pipes[i].height + PIPE_GAP ,pipes[i].x, PIPE_WIDTH, SCREEN_HEIGHT - pipes[i].height - PIPE_GAP, 1, true);
    }
}

//Função utilizada para atualizar a posição do passaro
void update_bird() {
    if (!gpio_get(BUTTON_PIN)) {
        bird.velocity = -2;
    } else {
        bird.velocity += 1;
    }
    
    bird.y += bird.velocity; // Corrigindo para mover no eixo Y
    if (bird.y < 0) bird.y = 0;
    if (bird.y > SCREEN_HEIGHT - 5) bird.y = SCREEN_HEIGHT - 5;
}

//Função utilizada para atualizar a posição dos obstáculos
void update_pipes() {
    for (int i = 0; i < OBSTACLES; i++) {
        pipes[i].x -= 2;
        
        if (pipes[i].x < 0){
            update_number_led(1, pio, sm);

            gpio_put(LED_G, 1);
            sleep_ms(20);
            gpio_put(LED_G, 0);


            pipes[i].x = SCREEN_WIDTH;
            pipes[i].height = rand() % (SCREEN_HEIGHT - PIPE_GAP - 10) + 5;
        }
    }
}

//Função utiliza para identificar se houve uma colisão do passaro com o obstáculo
int check_collision() {
    for (int i = 0; i < OBSTACLES; i++) {
        if ((bird.x + 5 > pipes[i].x && bird.x < pipes[i].x + PIPE_WIDTH) && 
            (bird.y < pipes[i].height || bird.y + 5 > pipes[i].height + PIPE_GAP)) {
            return 1;
        }
    }
    return 0;
}

//Função utilizada para fazer o jogo rodar em repetição e chamando as funções auxiliares
void game_loop() {
    while (true) {
        ssd1306_fill(&display, false);
        update_bird();
        update_pipes();
        draw_bird();
        draw_pipes();
        
        if (check_collision()) {
            gpio_put(LED_R, 1);
            sleep_ms(1000);
            gpio_put(LED_R, 0);
            
            reset_game();
        }

        ssd1306_send_data(&display);
        sleep_ms(50);
    }
}

//Função principal
int main() {
    init_hardware();
    bird.x = 20;
    bird.y = SCREEN_HEIGHT / 2;
    bird.velocity = 0;
    
    for (int i = 0; i < OBSTACLES; i++) {
        pipes[i].x = SCREEN_WIDTH + PIPE_SPACING * i;
        pipes[i].height = rand() % (SCREEN_HEIGHT - PIPE_GAP - 10) + 5;
    }
    
    gpio_set_irq_enabled_with_callback(botaoB, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    
    game_loop();
    
    return 0;
}
